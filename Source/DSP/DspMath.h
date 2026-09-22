#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
 #include <xmmintrin.h>
 #define HEAT_HAS_SSE 1
#endif

namespace heat::dsp
{
    inline constexpr double pi = 3.14159265358979323846;
    inline constexpr float minDb = -120.0f;

    inline float dbToGain (float db) noexcept
    {
        return std::exp (db * 0.11512925464970228f); // ln(10)/20
    }

    inline float gainToDb (float gain) noexcept
    {
        return gain > 1.0e-6f ? 8.685889638065037f * std::log (gain) : minDb; // 20/ln(10)
    }

    // Power (mean square) to dB.
    inline float powerToDb (float power) noexcept
    {
        return power > 1.0e-12f ? 4.3429448190325175f * std::log (power) : minDb; // 10/ln(10)
    }

    // One-pole smoothing coefficient for a time constant (time to reach 1 - 1/e
    // of a step). Sample-rate independent: the same time in ms produces the
    // same analogue response at any sample rate.
    inline float timeConstantCoeff (float timeMs, double sampleRate) noexcept
    {
        const double samples = std::max (1.0e-3, static_cast<double> (timeMs) * 1.0e-3 * sampleRate);
        return static_cast<float> (std::exp (-1.0 / samples));
    }

    template <typename T>
    inline T lerp (T a, T b, T t) noexcept { return a + (b - a) * t; }

    inline float clamp01 (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }

    // Exact float comparison (for "has this value changed at all" checks).
    inline bool exactlyEqual (float a, float b) noexcept
    {
        return std::equal_to<float>() (a, b);
    }

    inline bool isFiniteSample (float x) noexcept
    {
        return std::isfinite (x);
    }

    // Flushes denormals for the lifetime of the object (audio thread only).
    class ScopedFlushDenormals
    {
    public:
        ScopedFlushDenormals() noexcept
        {
           #if HEAT_HAS_SSE
            previous = _mm_getcsr();
            _mm_setcsr (previous | 0x8040); // FTZ | DAZ
           #endif
        }

        ~ScopedFlushDenormals() noexcept
        {
           #if HEAT_HAS_SSE
            _mm_setcsr (previous);
           #endif
        }

        ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
        ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

    private:
       #if HEAT_HAS_SSE
        unsigned int previous = 0;
       #endif
    };

    // Exponential (one-pole) parameter smoother with a fixed time constant.
    class OnePoleSmoother
    {
    public:
        void prepare (double sampleRate, float timeMs) noexcept
        {
            coeff = timeConstantCoeff (timeMs, sampleRate);
        }

        void reset (float value) noexcept { current = target = value; }
        void setTarget (float value) noexcept { target = value; }
        float getTarget() const noexcept { return target; }
        float getCurrent() const noexcept { return current; }

        float next() noexcept
        {
            current = target + coeff * (current - target);
            if (std::abs (current - target) < 1.0e-7f)
                current = target;
            return current;
        }

        // Advances n samples at once (for control-rate smoothing).
        float skip (int numSamples) noexcept
        {
            const float c = std::pow (coeff, static_cast<float> (numSamples));
            current = target + c * (current - target);
            if (std::abs (current - target) < 1.0e-7f)
                current = target;
            return current;
        }

        bool isSmoothing() const noexcept { return ! exactlyEqual (current, target); }

    private:
        float coeff = 0.0f;
        float current = 0.0f;
        float target = 0.0f;
    };

    // Linear ramp smoother (fixed ramp length). Reaches the target exactly.
    class LinearSmoother
    {
    public:
        void prepare (double sampleRate, float rampMs) noexcept
        {
            rampSamples = std::max (1, static_cast<int> (rampMs * 1.0e-3 * sampleRate));
        }

        void reset (float value) noexcept
        {
            current = target = value;
            step = 0.0f;
            remaining = 0;
        }

        void setTarget (float value) noexcept
        {
            if (exactlyEqual (value, target))
                return;
            target = value;
            remaining = rampSamples;
            step = (target - current) / static_cast<float> (rampSamples);
        }

        float next() noexcept
        {
            if (remaining > 0)
            {
                current += step;
                if (--remaining == 0)
                    current = target;
            }
            return current;
        }

        float getCurrent() const noexcept { return current; }
        float getTarget() const noexcept { return target; }
        bool isSmoothing() const noexcept { return remaining > 0; }

    private:
        float current = 0.0f, target = 0.0f, step = 0.0f;
        int rampSamples = 1, remaining = 0;
    };
}
