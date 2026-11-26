#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <random>
#include <cmath>
#include "LFO.h"

struct ModParams
{
    float pitch = 0.0f;
    float feedback = 0.85f;
    float pluck = 0.0f;
    float inharm = 0.0f;
    float inputCutoff = 8000.0f;
    float loopCutoff = 4000.0f;
    float dryWet = 0.5f;
    float jitter = 0.0f;
    float crossFb = 0.0f;
    float selfMod = 0.0f;
    float wetGain = 1.0f;
    float macro = 0.0f;
    bool gateEnabled = false;

    int lfo1Shape = 0, lfo1Sync = 0, lfo2Shape = 0, lfo2Sync = 0;
    float lfo1Rate = 1.0f, lfo1Phase = 0.0f, lfo1Pw = 0.5f, lfo1Smooth = 0.0f;
    float lfo2Rate = 1.0f, lfo2Phase = 0.0f, lfo2Pw = 0.5f, lfo2Smooth = 0.0f;
    bool lfo1Bypass = false, lfo2Bypass = false;

    float m1_pitch = 0.0f, m1_feed = 0.0f, m1_incut = 0.0f, m1_loopcut = 0.0f;
    float m1_hbeta = 0.0f, m1_inharm = 0.0f, m1_cross = 0.0f, m1_self = 0.0f;
    float m1_drift = 0.0f, m1_drywet = 0.0f, m1_wetgain = 0.0f, m1_macro = 0.0f;

    float m2_pitch = 0.0f, m2_feed = 0.0f, m2_incut = 0.0f, m2_loopcut = 0.0f;
    float m2_hbeta = 0.0f, m2_inharm = 0.0f, m2_cross = 0.0f, m2_self = 0.0f;
    float m2_drift = 0.0f, m2_drywet = 0.0f, m2_wetgain = 0.0f, m2_macro = 0.0f;

    float m1_lfo2 = 0.0f, m2_lfo1 = 0.0f;
};

class Resonator
{
public:
    Resonator();
    void prepare(double sampleRate, int samplesPerBlock, int numChans);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const ModParams& p, double bpm, bool isNegative);

    int getLatency() const;

private:
    static constexpr float baseFreqHz = 261.63f;
    static constexpr float maxDelaySamples = 16384.0f;
    static constexpr int numAllpassCells = 24;

    double currentSampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    struct DCBlocker
    {
        float x1 = 0.0f, y1 = 0.0f;
        static constexpr float R = 0.995f;
        float process(float x) noexcept
        {
            const float y = x - x1 + R * y1;
            x1 = x;
            y1 = y;
            return y;
        }
        void reset() noexcept { x1 = 0.0f; y1 = 0.0f; }
    };

    struct OnePoleLP
    {
        float a = 0.0f, z = 0.0f;
        void setCutoff(float cutoff, float sampleRate) noexcept
        {
            const float x = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / sampleRate);
            a = juce::jlimit(0.0f, 0.9995f, x);
        }
        float process(float input) noexcept
        {
            z = input * (1.0f - a) + z * a;
            return z;
        }
        void reset() noexcept { z = 0.0f; }
    };

    struct Allpass
    {
        float a = 0.0f;
        float x1 = 0.0f, y1 = 0.0f;
        void setCoeff(float coeff) noexcept { a = juce::jlimit(-0.99f, 0.99f, coeff); }
        float process(float x) noexcept
        {
            const float y = -a * x + x1 + a * y1;
            x1 = x;
            y1 = y;
            return y;
        }
        void reset() noexcept { x1 = 0.0f; y1 = 0.0f; }
    };

    struct ChannelState
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { (int)maxDelaySamples };
        juce::dsp::DelayLine<float> pluckDelay { 8192 };
        juce::dsp::StateVariableTPTFilter<float> inputFilter;
        OnePoleLP loopFilter;
        DCBlocker dcBlocker;
        OnePoleLP modFilter;
        Allpass inharmAllpass[numAllpassCells];
    };

    std::vector<ChannelState> channels;

    LFO lfo1, lfo2;
    float lfo1Value = 0.0f;
    float lfo2Value = 0.0f;

    juce::SmoothedValue<float> pitchSemitonesSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputCutoffSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> loopCutoffSmoothed;
    juce::SmoothedValue<float> dryWetSmoothed;
    juce::SmoothedValue<float> jitterSmoothed;
    juce::SmoothedValue<float> crossFbSmoothed;
    juce::SmoothedValue<float> selfModSmoothed;
    juce::SmoothedValue<float> wetGainSmoothed;
    juce::SmoothedValue<float> inharmSmoothed;
    juce::SmoothedValue<float> pluckSmoothed;
    juce::SmoothedValue<float> macroSmoothed;

    float lastAppliedInCutoff = -1.0f;
    float lastAppliedLoopCutoff = -1.0f;

    float inharmCoeffState = 0.0f;
    float inharmCoeffSmoothAlpha = 0.001f;

    float gateEnvelope = 1.0f;
    float gateAttackCoeff = 0.01f;
    float gateReleaseCoeff = 0.0001f;

    float driftFilterCoeff = 0.001f;
    float loopOutputs[2] = { 0.0f, 0.0f };

    std::mt19937 jitterGen;
    std::uniform_real_distribution<float> jitterDist { -1.0f, 1.0f };
    float jitterState[2] = { 0.0f, 0.0f };

    float semitonesToDelaySamples(float semitones, double sr) const noexcept;
    static float allpassPhaseDelay(float a, float omega) noexcept;
    static float maxAllpassCoeffForDelay(float maxTotalDelay, float omega) noexcept;
    void processInternal(juce::dsp::AudioBlock<float>& block, const ModParams& p, double bpm, bool isNegative, double sr);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Resonator)
};
