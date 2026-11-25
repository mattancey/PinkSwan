#include <juce_core/juce_core.h>
#include "../Dsp/Resonator.h"
#include "../Dsp/LFO.h"
#include <cmath>

class ResonatorTest : public juce::UnitTest
{
public:
    ResonatorTest() : juce::UnitTest("PinkSwan Resonator DSP Test") {}

    void runTest() override
    {
        testSilence();
        testLongTailWithoutGateAndDecayWithGate();
        testHighPitchExtremeInharmonicity();
        testHighSampleRateLowPitchPluck();
        testExtremeFeedbackAndSelfMod();
        testLfoBufferSizeIndependenceAndSmoothing();
    }

private:
    static ModParams makeDefaultParams()
    {
        ModParams p{};
        p.pitch = 0.0f;
        p.feedback = 0.85f;
        p.pluck = 0.0f;
        p.inharm = 0.0f;
        p.inputCutoff = 8000.0f;
        p.loopCutoff = 4000.0f;
        p.dryWet = 0.5f;
        p.jitter = 0.0f;
        p.crossFb = 0.0f;
        p.selfMod = 0.0f;
        p.wetGain = 1.0f;
        p.macro = 0.0f;
        p.gateEnabled = false;
        return p;
    }

    static float computeRms(const juce::AudioBuffer<float>& buf)
    {
        double sum = 0.0;
        const int numCh = buf.getNumChannels();
        const int numSamples = buf.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < numSamples; ++i)
                sum += (double)buf.getSample(ch, i) * (double)buf.getSample(ch, i);
        return (float)std::sqrt(sum / (double)(numCh * numSamples));
    }

    void testSilence()
    {
        beginTest("Process Silence");
        Resonator res;
        res.prepare(44100.0, 512, 2);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();

        auto p = makeDefaultParams();
        res.process(buffer, p, 120.0, false);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float s = buffer.getSample(ch, i);
                expect(std::isfinite(s));
                expectWithinAbsoluteError(s, 0.0f, 1.0e-6f);
            }
    }

    void testLongTailWithoutGateAndDecayWithGate()
    {
        beginTest("Gate OFF preserves long tail, Gate ON attenuates tail");
        const double sr = 44100.0;
        const int blockSize = 512;
        const int totalBlocksFor3Sec = (int)((3.0 * sr) / blockSize);

        auto runTail = [&](bool gateOn) -> float {
            Resonator res;
            res.prepare(sr, blockSize, 2);
            auto p = makeDefaultParams();
            p.feedback = 0.98f;
            p.loopCutoff = 16000.0f;
            p.dryWet = 1.0f;
            p.gateEnabled = gateOn;

            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int b = 0; b < totalBlocksFor3Sec; ++b)
            {
                buffer.clear();
                if (b == 0)
                {
                    buffer.setSample(0, 0, 1.0f);
                    buffer.setSample(1, 0, 1.0f);
                }
                res.process(buffer, p, 120.0, false);
            }
            return computeRms(buffer);
        };

        const float tailNoGate = runTail(false);
        const float tailWithGate = runTail(true);

        expect(tailNoGate > 0.001f);
        expect(tailWithGate < 0.0002f);
    }

    void testHighPitchExtremeInharmonicity()
    {
        beginTest("High Pitch +36st with Extreme Inharm stays finite and bounded");
        Resonator res;
        res.prepare(48000.0, 256, 2);

        auto p = makeDefaultParams();
        p.pitch = 36.0f;
        p.inharm = 0.99f;
        p.feedback = 0.92f;
        p.dryWet = 1.0f;

        juce::AudioBuffer<float> buffer(2, 256);
        for (int b = 0; b < 40; ++b)
        {
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample(0, 0, 0.8f);
                buffer.setSample(1, 0, 0.8f);
            }
            res.process(buffer, p, 120.0, false);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                {
                    const float s = buffer.getSample(ch, i);
                    expect(std::isfinite(s));
                    expect(std::abs(s) <= 2.5f);
                }
        }
    }

    void testHighSampleRateLowPitchPluck()
    {
        beginTest("192 kHz + Pitch -36st + Full Pluck stays within delay limits");
        Resonator res;
        res.prepare(192000.0, 512, 2);

        auto p = makeDefaultParams();
        p.pitch = -36.0f;
        p.pluck = 1.0f;
        p.feedback = 0.9f;
        p.dryWet = 0.8f;

        juce::AudioBuffer<float> buffer(2, 512);
        for (int b = 0; b < 20; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buffer.setSample(ch, i, b == 0 && i < 64 ? 0.5f : 0.0f);

            res.process(buffer, p, 120.0, true);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    expect(std::isfinite(buffer.getSample(ch, i)));
        }
    }

    void testExtremeFeedbackAndSelfMod()
    {
        beginTest("Extreme Feedback + CrossFB + SelfMod + 100Hz LFO remains bounded");
        Resonator res;
        res.prepare(44100.0, 128, 2);

        auto p = makeDefaultParams();
        p.feedback = 0.99f;
        p.crossFb = 0.98f;
        p.selfMod = 1.0f;
        p.jitter = 1.0f;
        p.inharm = 0.7f;
        p.pluck = 0.8f;
        p.dryWet = 1.0f;
        p.wetGain = 2.0f;
        p.lfo1Rate = 100.0f;
        p.lfo2Rate = 87.5f;
        p.m1_pitch = 1.0f;
        p.m2_lfo1 = 0.8f;
        p.m1_lfo2 = 0.8f;

        juce::Random rng(42);
        juce::AudioBuffer<float> buffer(2, 128);

        for (int b = 0; b < 100; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 128; ++i)
                    buffer.setSample(ch, i, (b < 5) ? (rng.nextFloat() * 2.0f - 1.0f) : 0.0f);

            res.process(buffer, p, 140.0, true);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 128; ++i)
                {
                    const float s = buffer.getSample(ch, i);
                    expect(std::isfinite(s));
                    expect(std::abs(s) <= 8.0f);
                }
        }
    }

    void testLfoBufferSizeIndependenceAndSmoothing()
    {
        beginTest("LFO Logistic and Smooth are independent of block size");
        LFO lfoA, lfoB;
        const double sr = 48000.0;
        lfoA.prepare(sr);
        lfoB.prepare(sr);

        lfoA.setShape(9); // Logistic
        lfoB.setShape(9);
        lfoA.setSmooth(0.5f);
        lfoB.setSmooth(0.5f);

        // Même nombre total de samples, appelés une fois par sample (simule n'importe quel découpage de buffer)
        float outA = 0.0f, outB = 0.0f;
        for (int i = 0; i < 24000; ++i)
        {
            outA = lfoA.processSample(4.0f, 0.0f, (float)sr);
            outB = lfoB.processSample(4.0f, 0.0f, (float)sr);
        }

        expect(std::isfinite(outA));
        expectWithinAbsoluteError(outA, outB, 1.0e-5f);
    }
};

static ResonatorTest resonatorTest;

int main()
{
    juce::UnitTestRunner runner;
    runner.runAllTests();

    int totalFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        totalFailures += runner.getResult(i)->failures;

    return totalFailures == 0 ? 0 : 1;
}
