#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <random>

class LFO
{
public:
    LFO() { gen.seed(std::random_device{}()); }

    void prepare(double sr) noexcept
    {
        sampleRate = sr > 1.0 ? sr : 44100.0;
        reset();
        updateSmoothCoeff();
    }

    void reset() noexcept
    {
        phase = 0.0f;
        shValue = 0.0f;
        smoothValue = 0.0f;
        logX = 0.37f;
        smoothedOutput = 0.0f;
    }

    void setShape(int shape) noexcept { currentShape = shape; }
    void setPulseWidth(float w) noexcept { pulseWidth = juce::jlimit(0.05f, 0.95f, w); }

    void setSmooth(float smoothAmount) noexcept
    {
        const float clamped = juce::jlimit(0.0f, 1.0f, smoothAmount);
        if (std::abs(clamped - currentSmoothAmount) > 1.0e-5f)
        {
            currentSmoothAmount = clamped;
            updateSmoothCoeff();
        }
    }

    float processSample(float frequencyHz, float phaseOffsetDegrees, float sr) noexcept
    {
        const float safeSr = sr > 1.0f ? sr : (float)sampleRate;
        const float freq = juce::jlimit(0.001f, safeSr * 0.45f, frequencyHz);

        phase += freq / safeSr;
        if (phase >= 1.0f)
        {
            phase -= std::floor(phase);
            shValue = dist(gen);
            logX = 3.65f * logX * (1.0f - logX);
            if (logX <= 0.01f || logX >= 0.99f)
                logX = 0.37f;
        }

        float p = phase + (phaseOffsetDegrees / 360.0f);
        p -= std::floor(p);

        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        constexpr float pi = juce::MathConstants<float>::pi;

        float val = 0.0f;
        switch (currentShape)
        {
            case 0: val = std::sin(p * twoPi); break;
            case 1: val = 4.0f * std::abs(p - 0.5f) - 1.0f; break;
            case 2: val = 2.0f * p - 1.0f; break;
            case 3: val = p < 0.5f ? 1.0f : -1.0f; break;
            case 4: val = p < pulseWidth ? 1.0f : -1.0f; break;
            case 5: val = p < 0.9f ? (p / 0.9f) * 2.0f - 1.0f : -1.0f; break;
            case 6: val = (1.0f - std::exp(-5.0f * p)) * 2.0f - 1.0f; break;
            case 7: val = shValue; break;
            case 8:
            {
                const float rndCoeff = 1.0f - std::exp(-twoPi * freq / safeSr);
                smoothValue += rndCoeff * (shValue - smoothValue);
                val = smoothValue;
                break;
            }
            case 9: val = logX * 2.0f - 1.0f; break;
            case 10: val = std::floor(p * 4.0f) / 3.0f * 2.0f - 1.0f; break;
            case 11: val = std::sin(p * 2.0f * twoPi); break;
            case 12:
            {
                val = std::sin(p * pi) * 1.5f;
                if (val > 1.0f) val = 2.0f - val;
                if (val < -1.0f) val = -2.0f - val;
                break;
            }
            default: val = 0.0f; break;
        }

        if (smoothCoeff >= 0.9999f)
            smoothedOutput = val;
        else
            smoothedOutput += smoothCoeff * (val - smoothedOutput);

        return smoothedOutput;
    }

private:
    void updateSmoothCoeff() noexcept
    {
        if (currentSmoothAmount <= 0.001f)
        {
            smoothCoeff = 1.0f;
        }
        else
        {
            const float tau = 0.001f + currentSmoothAmount * 0.149f;
            smoothCoeff = 1.0f - std::exp(-1.0f / (tau * (float)sampleRate));
        }
    }

    double sampleRate = 44100.0;
    float phase = 0.0f;
    int currentShape = 0;
    float pulseWidth = 0.5f;
    float shValue = 0.0f;
    float smoothValue = 0.0f;
    float logX = 0.37f;

    float currentSmoothAmount = 0.0f;
    float smoothCoeff = 1.0f;
    float smoothedOutput = 0.0f;

    std::mt19937 gen;
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
};
