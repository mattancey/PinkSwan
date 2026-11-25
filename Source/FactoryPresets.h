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
        // LFO sample par sample avec modulation croisée
        const float lfo1Hz = juce::jlimit(0.01f, 100.0f, lfo1HzBase + (lfo2Value * p.m2_lfo1 * 50.0f));
        const float lfo2Hz = juce::jlimit(0.01f, 100.0f, lfo2HzBase + (lfo1Value * p.m1_lfo2 * 50.0f));

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
        const float currentWetGain = mod(wetGainSmoothed.getNextValue(), p.m1_wetgain, p.m2_wetgain, 0.0f, 2.0f);

        // Mise à jour des filtres sans recalcul trigonométrique inutile à l'arrêt
        if (std::abs(inCut - lastAppliedInCutoff) > 0.05f)
        {
            for (int ch = 0; ch < activeChannels; ++ch)
                channels[(size_t)ch].inputFilter.setCutoffFrequency(inCut);
            lastAppliedInCutoff = inCut;
        }
        if (std::abs(loopCut - lastAppliedLoopCutoff) > 0.05f)
        {
            for (int ch = 0; ch < activeChannels; ++ch)
                channels[(size_t)ch].loopFilter.setCutoff(loopCut, (float)sr);
            lastAppliedLoopCutoff = loopCut;
        }

        const float baseDelaySamples = semitonesToDelaySamples(pitchSt, sr);

        // Calcul d'inharmonicité avec compensation exacte du délai de phase à la fréquence jouée
        float inharmCoeff = 0.0f;
        float delaySamples = baseDelaySamples;
        if (inharm > 0.001f && baseDelaySamples > 28.0f)
        {
            const float omega0 = juce::MathConstants<float>::twoPi / baseDelaySamples;
            float desiredA = juce::jlimit(0.0f, 0.95f, inharm * 0.95f);
            float totalPhaseDelay = (float)numAllpassCells * allpassPhaseDelay(desiredA, omega0);

            if (totalPhaseDelay > baseDelaySamples - 4.0f)
            {
                float lo = 0.0f;
                float hi = desiredA;
                for (int iter = 0; iter < 6; ++iter)
                {
                    const float mid = 0.5f * (lo + hi);
                    const float d = (float)numAllpassCells * allpassPhaseDelay(mid, omega0);
                    if (d <= baseDelaySamples - 4.0f)
                        lo = mid;
                    else
                        hi = mid;
                }
                desiredA = lo;
                totalPhaseDelay = (float)numAllpassCells * allpassPhaseDelay(desiredA, omega0);
            }

            inharmCoeff = desiredA;
            delaySamples = baseDelaySamples - totalPhaseDelay;
        }

        // Drift mono-cohérent avec la somme de 2 bruits lissés à 15 Hz (fidèle à BlueSwan, stable au SR)
        if (drift > 0.001f)
        {
            float driftSum = 0.0f;
            for (int j = 0; j < 2; ++j)
            {
                const float noise = jitterDist(jitterGen);
                jitterState[j] += driftFilterCoeff * (noise - jitterState[j]);
                driftSum += jitterState[j];
            }
            delaySamples += driftSum * drift * (float)(sr * 0.005);
        }

        // Self-modulation partagée sur la moyenne des canaux
        if (selfModAmt > 0.001f)
        {
            float modulator = 0.0f;
            for (int ch = 0; ch < activeChannels; ++ch)
                modulator += channels[(size_t)ch].modFilter.process(loopOutputs[ch]);
            modulator /= (float)activeChannels;
            delaySamples += modulator * selfModAmt * (float)(sr * 0.01);
        }

        delaySamples = juce::jlimit(4.0f, maxDelaySamples - 8.0f, delaySamples);

        // Gate optionnel sans clic (seuil -48 dB = 0.00398107, relâchement 2.0 s)
        float inputAbs = 0.0f;
        for (int ch = 0; ch < activeChannels; ++ch)
            inputAbs = juce::jmax(inputAbs, std::abs(channelData[ch][i]));

        const float gateTarget = (!p.gateEnabled || inputAbs > 0.00398107f) ? 1.0f : 0.0f;
        const float gateCoeff = gateTarget > gateEnvelope ? gateAttackCoeff : gateReleaseCoeff;
        gateEnvelope += gateCoeff * (gateTarget - gateEnvelope);

        // 1) Lecture de la boucle de résonance pour chaque canal
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto& state = channels[(size_t)ch];
            state.delayLine.setDelay(delaySamples);
            float delayed = state.delayLine.popSample(0);

            if (inharmCoeff > 0.0005f)
            {
                for (int j = 0; j < numAllpassCells; ++j)
                {
                    state.inharmAllpass[j].setCoeff(inharmCoeff);
                    delayed = state.inharmAllpass[j].process(delayed);
                }
            }

            float loopFiltered = state.loopFilter.process(delayed);
            loopFiltered = state.dcBlocker.process(loopFiltered);
            loopOutputs[ch] = loopFiltered;
        }

        // 2) Excitation, cross-feedback, écriture dans la ligne à retard et mix final
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto& state = channels[(size_t)ch];
            const float dry = channelData[ch][i];

            float crossFeedbackSignal = 0.0f;
            if (activeChannels == 2)
                crossFeedbackSignal = loopOutputs[1 - ch] * crossFB;

            float pluckFiltered = dry;
            if (pluck > 0.001f)
            {
                const float pluckDelayTime = juce::jlimit(2.0f, 8184.0f, delaySamples * 0.5f);
                state.pluckDelay.setDelay(pluckDelayTime);
                const float pluckDelayed = state.pluckDelay.popSample(0);
                const float mixAmt = juce::jlimit(0.0f, 1.0f, pluck * 1.2f);
                pluckFiltered = dry * (1.0f - mixAmt) + (dry - pluckDelayed) * mixAmt;
                pluckFiltered *= (1.0f + (pluck * 0.5f));
                state.pluckDelay.pushSample(0, dry);
            }

            const float inputFiltered = state.inputFilter.processSample(0, pluckFiltered);
            float toWrite = inputFiltered + (loopOutputs[ch] * fb) + crossFeedbackSignal;
            toWrite = std::tanh(toWrite);

            state.delayLine.pushSample(0, toWrite);

            float wetSignal = loopOutputs[ch] + crossFeedbackSignal;
            wetSignal *= currentWetGain * gateEnvelope;

            channelData[ch][i] = dry * (1.0f - wetMix) + wetSignal * wetMix;
        }
    }
}

float Resonator::semitonesToDelaySamples(float semitones, double sr) const noexcept
{
    const float freq = baseFreqHz * std::pow(2.0f, semitones / 12.0f);
    const float delay = (float)(sr / freq);
    return juce::jlimit(4.0f, maxDelaySamples - 8.0f, delay);
}
