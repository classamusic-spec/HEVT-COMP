#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // Detector-path EQ (never touches the audible signal):
    //
    //   HPF  2nd-order Butterworth high-pass, 20 … 400 Hz
    //   LPF  2nd-order Butterworth low-pass, 1 … 20 kHz; at the top of its
    //        range it fades out and is bypassed exactly (20 kHz = off)
    //   BELL peaking band (frequency, gain ±18 dB, Q); 0 dB is an exact bypass.
    //        A positive bell makes the compressor react more to that band
    //        (de-essing, taming a resonance); a negative one less.
    //
    // All three are topology-preserving-transform state-variable filters:
    // unconditionally stable and click-free under modulation. Frequencies are
    // smoothed in the log domain, gain in dB and Q in the log domain;
    // coefficients are refreshed every `updateInterval` samples.
    class SidechainFilter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int updateInterval = 16;
        static constexpr float lpfOffHz = 20000.0f;   // LPF bypassed at/above
        static constexpr float lpfFadeStartHz = 16000.0f;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // Snaps every control without smoothing (used on prepare / state load).
        void setCutoffImmediate (float hz) noexcept;
        void setLowpassImmediate (float hz) noexcept;
        void setBellImmediate (float hz, float gainDb, float q) noexcept;

        void setCutoff (float hz) noexcept;           // HPF
        void setLowpass (float hz) noexcept;
        void setBell (float hz, float gainDb, float q) noexcept;

        // Processes in place. numChannels <= maxChannels.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        float getCurrentCutoff() const noexcept;

        // Magnitude responses of the designs; used by tests and measurements.
        static double magnitudeAt (double hz, double cutoffHz, double sampleRate) noexcept; // HPF
        static double lowpassMagnitudeAt (double hz, double cutoffHz, double sampleRate) noexcept;
        static double bellMagnitudeAt (double hz, double centreHz, double gainDb, double q, double sampleRate) noexcept;
        // Complete detector-path response for a set of controls.
        static double chainMagnitudeAt (double hz, double hpfHz, double lpfHz, double bellHz, double bellDb,
                                        double bellQ, double sampleRate) noexcept;
        // Wet amount of the LPF for a cutoff (1 = fully active, 0 = bypassed).
        static float lowpassAmountFor (float hz) noexcept;

    private:
        struct Svf
        {
            float g = 0.0f, k = 1.41421356f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
            float ic1eq[maxChannels] {}, ic2eq[maxChannels] {};

            void design (double fc, double sampleRate, double kk) noexcept;
            void reset() noexcept;

            // Returns v1 (band-pass state) and v2 (low-pass state).
            void tick (int ch, float v0, float& v1, float& v2) noexcept
            {
                const float v3 = v0 - ic2eq[ch];
                v1 = a1 * ic1eq[ch] + a2 * v3;
                v2 = ic2eq[ch] + a2 * ic1eq[ch] + a3 * v3;
                ic1eq[ch] = 2.0f * v1 - ic1eq[ch];
                ic2eq[ch] = 2.0f * v2 - ic2eq[ch];
            }
        };

        void updateCoefficients() noexcept;
        bool isSmoothing() const noexcept;

        double fs = 48000.0;
        OnePoleSmoother logCutoff, logLowpass, logBellHz, bellDb, logBellQ;
        Svf hpf, lpf, bell;
        float lpfAmount = 0.0f;
        float bellM1 = 0.0f;   // k · (A² − 1)
        int counter = 0;
    };
}
