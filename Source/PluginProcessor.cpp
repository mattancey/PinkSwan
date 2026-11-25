#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

PinkSwanAudioProcessor::PinkSwanAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", Params::createLayout()),
      presetManager(apvts)
{
    auto raw = [&](const char* id) { return apvts.getRawParameterValue(id); };

    ptrs.pitch       = raw(Params::pitch);
    ptrs.feedback    = raw(Params::feedback);
    ptrs.inputCutoff = raw(Params::inputCutoff);
    ptrs.loopCutoff  = raw(Params::loopCutoff);
    ptrs.dryWet      = raw(Params::dryWet);
    ptrs.macro       = raw(Params::macro);
    ptrs.jitter      = raw(Params::jitter);
    ptrs.crossFb     = raw(Params::crossFb);
    ptrs.polarity    = raw(Params::polarity);
    ptrs.gateEnabled = raw(Params::gateEnabled);
    ptrs.selfMod     = raw(Params::selfMod);
    ptrs.wetGain     = raw(Params::wetGain);
    ptrs.pluck       = raw(Params::pluck);
    ptrs.inharm      = raw(Params::inharm);

    ptrs.lfo1Bypass = raw(Params::lfo1Bypass);
    ptrs.lfo1Sync   = raw(Params::lfo1Sync);
    ptrs.lfo1Rate   = raw(Params::lfo1Rate);
    ptrs.lfo1Phase  = raw(Params::lfo1Phase);
    ptrs.lfo1Shape  = raw(Params::lfo1Shape);
    ptrs.lfo1Pw     = raw(Params::lfo1Pw);
    ptrs.lfo1Smooth = raw(Params::lfo1Smooth);

    ptrs.lfo2Bypass = raw(Params::lfo2Bypass);
    ptrs.lfo2Sync   = raw(Params::lfo2Sync);
    ptrs.lfo2Rate   = raw(Params::lfo2Rate);
    ptrs.lfo2Phase  = raw(Params::lfo2Phase);
    ptrs.lfo2Shape  = raw(Params::lfo2Shape);
    ptrs.lfo2Pw     = raw(Params::lfo2Pw);
    ptrs.lfo2Smooth = raw(Params::lfo2Smooth);

    ptrs.m1_pitch   = raw(Params::m1_pitch);   ptrs.m2_pitch   = raw(Params::m2_pitch);
    ptrs.m1_feed    = raw(Params::m1_feed);    ptrs.m2_feed    = raw(Params::m2_feed);
    ptrs.m1_incut   = raw(Params::m1_incut);   ptrs.m2_incut   = raw(Params::m2_incut);
    ptrs.m1_loopcut = raw(Params::m1_loopcut); ptrs.m2_loopcut = raw(Params::m2_loopcut);
    ptrs.m1_hbeta   = raw(Params::m1_hbeta);   ptrs.m2_hbeta   = raw(Params::m2_hbeta);
    ptrs.m1_inharm  = raw(Params::m1_inharm);  ptrs.m2_inharm  = raw(Params::m2_inharm);
    ptrs.m1_cross   = raw(Params::m1_cross);   ptrs.m2_cross   = raw(Params::m2_cross);
    ptrs.m1_self    = raw(Params::m1_self);    ptrs.m2_self    = raw(Params::m2_self);
    ptrs.m1_drift   = raw(Params::m1_drift);   ptrs.m2_drift   = raw(Params::m2_drift);
    ptrs.m1_drywet  = raw(Params::m1_drywet);  ptrs.m2_drywet  = raw(Params::m2_drywet);
    ptrs.m1_wetgain = raw(Params::m1_wetgain); ptrs.m2_wetgain = raw(Params::m2_wetgain);
    ptrs.m1_macro   = raw(Params::m1_macro);   ptrs.m2_macro   = raw(Params::m2_macro);
    ptrs.m1_lfo2    = raw(Params::m1_lfo2);    ptrs.m2_lfo1    = raw(Params::m2_lfo1);
}

PinkSwanAudioProcessor::~PinkSwanAudioProcessor() {}

void PinkSwanAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resonator.prepare(sampleRate, samplesPerBlock, 2);
    setLatencySamples(resonator.getLatency());
}

void PinkSwanAudioProcessor::releaseResources()
{
    resonator.reset();
}

bool PinkSwanAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PinkSwanAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (panicRequested.exchange(false, std::memory_order_acq_rel))
        resonator.reset();

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto bpmOpt = pos->getBpm())
                if (*bpmOpt > 0.0)
                    bpm = *bpmOpt;

    auto load = [](const std::atomic<float>* a) noexcept {
        return a->load(std::memory_order_relaxed);
    };

    ModParams p{};
    p.pitch       = load(ptrs.pitch);
    p.feedback    = load(ptrs.feedback);
    p.pluck       = load(ptrs.pluck);
    p.inharm      = load(ptrs.inharm);
    p.inputCutoff = load(ptrs.inputCutoff);
    p.loopCutoff  = load(ptrs.loopCutoff);
    p.dryWet      = load(ptrs.dryWet);
    p.jitter      = load(ptrs.jitter);
    p.crossFb     = load(ptrs.crossFb);
    p.selfMod     = load(ptrs.selfMod);
    p.wetGain     = load(ptrs.wetGain);
    p.macro       = load(ptrs.macro);
    p.gateEnabled = load(ptrs.gateEnabled) > 0.5f;

    p.lfo1Bypass = load(ptrs.lfo1Bypass) > 0.5f;
    p.lfo1Sync   = (int)load(ptrs.lfo1Sync);
    p.lfo1Rate   = load(ptrs.lfo1Rate);
    p.lfo1Phase  = load(ptrs.lfo1Phase);
    p.lfo1Shape  = (int)load(ptrs.lfo1Shape);
    p.lfo1Pw     = load(ptrs.lfo1Pw);
    p.lfo1Smooth = load(ptrs.lfo1Smooth);

    p.lfo2Bypass = load(ptrs.lfo2Bypass) > 0.5f;
    p.lfo2Sync   = (int)load(ptrs.lfo2Sync);
    p.lfo2Rate   = load(ptrs.lfo2Rate);
    p.lfo2Phase  = load(ptrs.lfo2Phase);
    p.lfo2Shape  = (int)load(ptrs.lfo2Shape);
    p.lfo2Pw     = load(ptrs.lfo2Pw);
    p.lfo2Smooth = load(ptrs.lfo2Smooth);

    p.m1_pitch   = load(ptrs.m1_pitch);   p.m2_pitch   = load(ptrs.m2_pitch);
    p.m1_feed    = load(ptrs.m1_feed);    p.m2_feed    = load(ptrs.m2_feed);
    p.m1_incut   = load(ptrs.m1_incut);   p.m2_incut   = load(ptrs.m2_incut);
    p.m1_loopcut = load(ptrs.m1_loopcut); p.m2_loopcut = load(ptrs.m2_loopcut);
    p.m1_hbeta   = load(ptrs.m1_hbeta);   p.m2_hbeta   = load(ptrs.m2_hbeta);
    p.m1_inharm  = load(ptrs.m1_inharm);  p.m2_inharm  = load(ptrs.m2_inharm);
    p.m1_cross   = load(ptrs.m1_cross);   p.m2_cross   = load(ptrs.m2_cross);
    p.m1_self    = load(ptrs.m1_self);    p.m2_self    = load(ptrs.m2_self);
    p.m1_drift   = load(ptrs.m1_drift);   p.m2_drift   = load(ptrs.m2_drift);
    p.m1_drywet  = load(ptrs.m1_drywet);  p.m2_drywet  = load(ptrs.m2_drywet);
    p.m1_wetgain = load(ptrs.m1_wetgain); p.m2_wetgain = load(ptrs.m2_wetgain);
    p.m1_macro   = load(ptrs.m1_macro);   p.m2_macro   = load(ptrs.m2_macro);
    p.m1_lfo2    = load(ptrs.m1_lfo2);    p.m2_lfo1    = load(ptrs.m2_lfo1);

    const bool isNeg = load(ptrs.polarity) > 0.5f;
    resonator.process(buffer, p, bpm, isNeg);
}

juce::AudioProcessorEditor* PinkSwanAudioProcessor::createEditor()
{
    return new PinkSwanAudioProcessorEditor(*this);
}

void PinkSwanAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PinkSwanAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        stateRevision.fetch_add(1, std::memory_order_release);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PinkSwanAudioProcessor();
}
