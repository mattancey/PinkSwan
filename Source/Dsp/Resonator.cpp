#include "Resonator.h"

Resonator::Resonator()
{
    pitchSemitonesSmoothed.setCurrentAndTargetValue(0.0f);
    feedbackSmoothed.setCurrentAndTargetValue(0.85f);
    inputCutoffSmoothed.setCurrentAndTargetValue(8000.0f);
    loopCutoffSmoothed.setCurrentAndTargetValue(4000.0f);
    dryWetSmoothed.setCurrentAndTargetValue(0.5f);
    jitterSmoothed.setCurrentAndTargetValue(0.0f);
    crossFbSmoothed.setCurrentAndTargetValue(0.0f);
    selfModSmoothed.setCurrentAndTargetValue(0.0f);
    wetGainSmoothed.setCurrentAndTargetValue(1.0f);
    inharmSmoothed.setCurrentAndTargetValue(0.0f);
    pluckSmoothed.setCurrentAndTargetValue(0.0f);
    macroSmoothed.setCurrentAndTargetValue(0.0f);

    jitterGen.seed(std::random_device{}());
}

void Resonator::prepare(double sampleRate, int samplesPerBlock, int numChans)
{
    currentSampleRate = sampleRate > 1.0 ? sampleRate : 44100.0;
    blockSize = juce::jmax(1, samplesPerBlock);
    numChannels = juce::jlimit(1, 2, numChans);

    const double osSampleRate = currentSampleRate * 2.0;
    const juce::uint32 osBlockSize = (juce::uint32)(blockSize * 2);
    juce::dsp::ProcessSpec monoSpec { osSampleRate, osBlockSize, 1 };

    channels.resize((size_t)numChannels);
    for (auto& ch : channels)
    {
        ch.delayLine.prepare(monoSpec);
        ch.delayLine.reset();
        ch.pluckDelay.prepare(monoSpec);
        ch.pluckDelay.reset();
        ch.inputFilter.prepare(monoSpec);
        ch.inputFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        ch.inputFilter.reset();
        ch.loopFilter.reset();
        ch.dcBlocker.reset();
        ch.modFilter.setCutoff(20.0f, (float)osSampleRate);
        ch.modFilter.reset();
        for (int i = 0; i < numAllpassCells; ++i)
            ch.inharmAllpass[i].reset();
    }

    lfo1.prepare(osSampleRate);
    lfo2.prepare(osSampleRate);
    lfo1Value = 0.0f;
    lfo2Value = 0.0f;

    pitchSemitonesSmoothed.reset(osSampleRate, 0.03);
    feedbackSmoothed.reset(osSampleRate, 0.02);
    inputCutoffSmoothed.reset(osSampleRate, 0.02);
    loopCutoffSmoothed.reset(osSampleRate, 0.02);
    dryWetSmoothed.reset(osSampleRate, 0.02);
    jitterSmoothed.reset(osSampleRate, 0.05);
    crossFbSmoothed.reset(osSampleRate, 0.03);
    selfModSmoothed.reset(osSampleRate, 0.03);
    wetGainSmoothed.reset(osSampleRate, 0.02);
    inharmSmoothed.reset(osSampleRate, 0.03);
    pluckSmoothed.reset(osSampleRate, 0.03);
    macroSmoothed.reset(osSampleRate, 0.03);

    lastAppliedInCutoff = -1.0f;
    lastAppliedLoopCutoff = -1.0f;

    // Enveloppe du gate : ouverture douce (~3 ms, zéro clic), relâchement de ~2.0 s (-60 dB en 2.0 s -> tau = 2.0 / 6.91)
    gateEnvelope = 1.0f;
    gateAttackCoeff = 1.0f - std::exp(-1.0f / (0.003f * (float)osSampleRate));
    gateReleaseCoeff = 1.0f - std::exp(-1.0f / (0.29f * (float)osSampleRate));

    // Filtre de drift normalisé à 15 Hz (équivalent exact de 0.001f à 48 kHz X2)
    driftFilterCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 15.0f / (float)osSampleRate);

    loopOutputs[0] = 0.0f;
    loopOutputs[1] = 0.0f;
    jitterState[0] = 0.0f;
    jitterState[1] = 0.0f;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing((size_t)blockSize);
    oversampler->reset();
}

void Resonator::reset()
{
    for (auto& ch : channels)
    {
        ch.delayLine.reset();
        ch.pluckDelay.reset();
        ch.inputFilter.reset();
        ch.loopFilter.reset();
        ch.dcBlocker.reset();
        ch.modFilter.reset();
        for (int i = 0; i < numAllpassCells; ++i)
            ch.inharmAllpass[i].reset();
    }

    loopOutputs[0] = 0.0f;
    loopOutputs[1] = 0.0f;
    jitterState[0] = 0.0f;
    jitterState[1] = 0.0f;
    gateEnvelope = 1.0f;
    lastAppliedInCutoff = -1.0f;
    lastAppliedLoopCutoff = -1.0f;

    if (oversampler)
        oversampler->reset();
}

int Resonator::getLatency() const
{
    if (oversampler)
        return (int)oversampler->getLatencyInSamples();
    return 0;
}

void Resonator::process(juce::AudioBuffer<float>& buffer, const ModParams& p, double bpm, bool isNegative)
{
    if (!oversampler)
        return;

    juce::dsp::AudioBlock<float> block(buffer);
    auto upBlock = oversampler->processSamplesUp(block);
    processInternal(upBlock, p, bpm, isNegative, currentSampleRate * 2.0);
    oversampler->processSamplesDown(block);
}

float Resonator::allpassPhaseDelay(float a, float omega) noexcept
{
    if (omega < 1.0e-4f)
        return (1.0f + a) / (1.0f - a + 1.0e-5f);

    const float sinW = std::sin(omega);
    const float cosW = std::cos(omega);
    return 1.0f + (2.0f / omega) * std::atan2(a * sinW, 1.0f - a * cosW);
}

void Resonator::processInternal(juce::dsp::AudioBlock<float>& block, const ModParams& p, double bpm, bool isNegative, double sr)
{
    const int activeChannels = juce::jmin((int)channels.size(), (int)block.getNumChannels());
    const int numSamples = (int)block.getNumSamples();
    if (activeChannels <= 0 || numSamples <= 0)
        return;

    const float polarityMultiplier = isNegative ? -1.0f : 1.0f;
    const float safeBpm = (float)juce::jlimit(20.0, 400.0, bpm);

    auto getLfoHz = [&](int syncIdx, float rate) noexcept -> float {
        if (syncIdx == 0)
            return rate;

        const float beatsPerSec = safeBpm / 60.0f;
        float beatsPerCycle = 1.0f;
        switch (syncIdx)
        {
            case 1:  beatsPerCycle = 32.0f; break;                // 8 bars
            case 2:  beatsPerCycle = 24.0f; break;                // 6 bars
            case 3:  beatsPerCycle = 16.0f; break;                // 4 bars
            case 4:  beatsPerCycle = 8.0f; break;                 // 2 bars
            case 5:  beatsPerCycle = 4.0f; break;                 // 1 bar
            case 6:  beatsPerCycle = 4.0f * (2.0f / 3.0f); break; // 1/1 T
            case 7:  beatsPerCycle = 4.0f * 1.5f; break;          // 1/1 D
            case 8:  beatsPerCycle = 2.0f; break;                 // 1/2
            case 9:  beatsPerCycle = 2.0f * (2.0f / 3.0f); break; // 1/2 T
            case 10: beatsPerCycle = 2.0f * 1.5f; break;          // 1/2 D
            case 11: beatsPerCycle = 1.0f; break;                 // 1/4
            case 12: beatsPerCycle = 2.0f / 3.0f; break;          // 1/4 T
            case 13: beatsPerCycle = 1.5f; break;                 // 1/4 D
            case 14: beatsPerCycle = 0.5f; break;                 // 1/8
            case 15: beatsPerCycle = 0.5f * (2.0f / 3.0f); break; // 1/8 T
            case 16: beatsPerCycle = 0.5f * 1.5f; break;          // 1/8 D
            case 17: beatsPerCycle = 0.25f; break;                // 1/16
            case 18: beatsPerCycle = 0.25f * (2.0f / 3.0f); break;// 1/16 T
            case 19: beatsPerCycle = 0.25f * 1.5f; break;         // 1/16 D
            case 20: beatsPerCycle = 5.0f; break;                 // 5/4
            case 21: beatsPerCycle = 6.0f; break;                 // 6/4
            case 22: beatsPerCycle = 7.0f; break;                 // 7/4
            case 23: beatsPerCycle = 9.0f; break;                 // 9/4
            case 24: beatsPerCycle = 18.0f; break;                // 18/4
            default: beatsPerCycle = 1.0f; break;
        }
        return beatsPerSec / beatsPerCycle;
    };

    const float lfo1HzBase = getLfoHz(p.lfo1Sync, p.lfo1Rate);
    const float lfo2HzBase = getLfoHz(p.lfo2Sync, p.lfo2Rate);

    // Courbe exponentielle symétrique sur les profondeurs de modulation croisée (-1..+1 -> ±5 octaves)
    auto expDepthOctaves = [](float d) noexcept -> float {
        constexpr float k = 3.0f;
        constexpr float denom = 19.085536923f; // exp(3.0) - 1.0
        const float mag = (std::exp(k * std::abs(d)) - 1.0f) / denom;
        return std::copysign(mag, d) * 5.0f;
    };
    const float crossOctM2ToLfo1 = expDepthOctaves(p.m2_lfo1);
    const float crossOctM1ToLfo2 = expDepthOctaves(p.m1_lfo2);

    lfo1.setShape(p.lfo1Shape);
    lfo1.setPulseWidth(p.lfo1Pw);
    lfo1.setSmooth(p.lfo1Smooth);

    lfo2.setShape(p.lfo2Shape);
    lfo2.setPulseWidth(p.lfo2Pw);
    lfo2.setSmooth(p.lfo2Smooth);

    pitchSemitonesSmoothed.setTargetValue(p.pitch);
    feedbackSmoothed.setTargetValue(p.feedback);
    inputCutoffSmoothed.setTargetValue(juce::jlimit(20.0f, 20000.0f, p.inputCutoff));
    loopCutoffSmoothed.setTargetValue(juce::jlimit(20.0f, 20000.0f, p.loopCutoff));
    dryWetSmoothed.setTargetValue(p.dryWet);
    jitterSmoothed.setTargetValue(p.jitter);
    crossFbSmoothed.setTargetValue(p.crossFb);
    selfModSmoothed.setTargetValue(p.selfMod);
    wetGainSmoothed.setTargetValue(p.wetGain);
    inharmSmoothed.setTargetValue(p.inharm);
    pluckSmoothed.setTargetValue(p.pluck);
    macroSmoothed.setTargetValue(p.macro);

    float* channelData[2] = { nullptr, nullptr };
    for (int ch = 0; ch < activeChannels; ++ch)
        channelData[ch] = block.getChannelPointer((size_t)ch);

    for (int i = 0; i < numSamples; ++i)
    {
        // LFO sample par sample avec modulation croisée exponentielle (en octaves)
        const float lfo1Hz = juce::jlimit(0.01f, 100.0f, lfo1HzBase * std::exp2(lfo2Value * crossOctM2ToLfo1));
        const float lfo2Hz = juce::jlimit(0.01f, 100.0f, lfo2HzBase * std::exp2(lfo1Value * crossOctM1ToLfo2));

        lfo1Value = p.lfo1Bypass ? 0.0f : lfo1.processSample(lfo1Hz, p.lfo1Phase, (float)sr);
        lfo2Value = p.lfo2Bypass ? 0.0f : lfo2.processSample(lfo2Hz, p.lfo2Phase, (float)sr);

        const float l1 = lfo1Value;
        const float l2 = lfo2Value;

        auto mod = [&](float base, float d1, float d2, float minVal, float maxVal) noexcept -> float {
            return juce::jlimit(minVal, maxVal, base + (l1 * d1) + (l2 * d2));
        };

        const float baseMacro = macroSmoothed.getNextValue();
        const float mMacro = mod(baseMacro, p.m1_macro, p.m2_macro, 0.0f, 1.0f);
        const float macroBoost = mMacro > 0.001f ? mMacro : 0.0f;

        const float pitchSt = mod(pitchSemitonesSmoothed.getNextValue() + (macroBoost * 24.0f),
                                  p.m1_pitch * 24.0f, p.m2_pitch * 24.0f, -36.0f, 36.0f);
        const float fb = mod(feedbackSmoothed.getNextValue() + (macroBoost * 0.1f),
                             p.m1_feed, p.m2_feed, 0.0f, 0.98f) * polarityMultiplier;
        const float inCut = mod(inputCutoffSmoothed.getNextValue() * (1.0f + macroBoost * 3.0f),
                                p.m1_incut * 10000.0f, p.m2_incut * 10000.0f, 20.0f, 20000.0f);
        const float loopCut = mod(loopCutoffSmoothed.getNextValue() * (1.0f + macroBoost * 3.0f),
                                  p.m1_loopcut * 10000.0f, p.m2_loopcut * 10000.0f, 20.0f, 20000.0f);
        const float pluck = mod(pluckSmoothed.getNextValue(), p.m1_hbeta, p.m2_hbeta, 0.0f, 1.0f);
        const float inharm = mod(inharmSmoothed.getNextValue(), p.m1_inharm, p.m2_inharm, 0.0f, 0.99f);
        const float crossFB = mod(crossFbSmoothed.getNextValue(), p.m1_cross, p.m2_cross, 0.0f, 0.98f);
        const float selfModAmt = mod(selfModSmoothed.getNextValue(), p.m1_self, p.m2_self, 0.0f, 1.0f);
        const float drift = mod(jitterSmoothed.getNextValue(), p.m1_drift, p.m2_drift, 0.0f, 1.0f);
        const float wetMix = mod(dryWetSmoothed.getNextValue(), p.m1_drywet, p.m2_drywet, 0.0f, 1.0f);
