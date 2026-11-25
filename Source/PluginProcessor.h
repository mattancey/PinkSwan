#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Dsp/Resonator.h"
#include "Presets.h"

class PinkSwanAudioProcessor : public juce::AudioProcessor
{
public:
    PinkSwanAudioProcessor();
    ~PinkSwanAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void triggerPanic() noexcept { panicRequested.store(true, std::memory_order_release); }
    uint32_t getStateRevision() const noexcept { return stateRevision.load(std::memory_order_acquire); }

    pinkpresets::PresetManager& getPresetManager() noexcept { return presetManager; }

    juce::AudioProcessorValueTreeState apvts;

private:
    struct ParamPointers
    {
        std::atomic<float>* pitch = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* inputCutoff = nullptr;
        std::atomic<float>* loopCutoff = nullptr;
        std::atomic<float>* dryWet = nullptr;
        std::atomic<float>* macro = nullptr;
        std::atomic<float>* jitter = nullptr;
        std::atomic<float>* crossFb = nullptr;
        std::atomic<float>* polarity = nullptr;
        std::atomic<float>* gateEnabled = nullptr;
        std::atomic<float>* selfMod = nullptr;
        std::atomic<float>* wetGain = nullptr;
        std::atomic<float>* pluck = nullptr;
        std::atomic<float>* inharm = nullptr;

        std::atomic<float>* lfo1Bypass = nullptr;
        std::atomic<float>* lfo1Sync = nullptr;
        std::atomic<float>* lfo1Rate = nullptr;
        std::atomic<float>* lfo1Phase = nullptr;
        std::atomic<float>* lfo1Shape = nullptr;
        std::atomic<float>* lfo1Pw = nullptr;
        std::atomic<float>* lfo1Smooth = nullptr;

        std::atomic<float>* lfo2Bypass = nullptr;
        std::atomic<float>* lfo2Sync = nullptr;
        std::atomic<float>* lfo2Rate = nullptr;
        std::atomic<float>* lfo2Phase = nullptr;
        std::atomic<float>* lfo2Shape = nullptr;
        std::atomic<float>* lfo2Pw = nullptr;
        std::atomic<float>* lfo2Smooth = nullptr;

        std::atomic<float>* m1_pitch = nullptr;   std::atomic<float>* m2_pitch = nullptr;
        std::atomic<float>* m1_feed = nullptr;    std::atomic<float>* m2_feed = nullptr;
        std::atomic<float>* m1_incut = nullptr;   std::atomic<float>* m2_incut = nullptr;
        std::atomic<float>* m1_loopcut = nullptr; std::atomic<float>* m2_loopcut = nullptr;
        std::atomic<float>* m1_hbeta = nullptr;   std::atomic<float>* m2_hbeta = nullptr;
        std::atomic<float>* m1_inharm = nullptr;  std::atomic<float>* m2_inharm = nullptr;
        std::atomic<float>* m1_cross = nullptr;   std::atomic<float>* m2_cross = nullptr;
        std::atomic<float>* m1_self = nullptr;    std::atomic<float>* m2_self = nullptr;
        std::atomic<float>* m1_drift = nullptr;   std::atomic<float>* m2_drift = nullptr;
        std::atomic<float>* m1_drywet = nullptr;  std::atomic<float>* m2_drywet = nullptr;
        std::atomic<float>* m1_wetgain = nullptr; std::atomic<float>* m2_wetgain = nullptr;
        std::atomic<float>* m1_macro = nullptr;   std::atomic<float>* m2_macro = nullptr;
        std::atomic<float>* m1_lfo2 = nullptr;    std::atomic<float>* m2_lfo1 = nullptr;
    };

    ParamPointers ptrs;
    Resonator resonator;
    pinkpresets::PresetManager presetManager;

    std::atomic<bool> panicRequested { false };
    std::atomic<uint32_t> stateRevision { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinkSwanAudioProcessor)
};
