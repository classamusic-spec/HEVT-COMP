#pragma once

#include <vector>

namespace heat::dsp
{
    // Output limiter with true-peak detection and a guaranteed sample ceiling.
    //
    //   detect   p(m) = max over channels of |x(m)| and the inter-sample
    //            peaks on both sides of m (8x polyphase windowed-sinc
    //            interpolation, 48 taps per phase so content near Nyquist is
    //            not under-read; BS.1770 notes 4x under-reads by up to
    //            0.69 dB, 8x by 0.17 dB)
    //   require  r(m) = min(1, ceiling / p(m))
    //   release  r' follows r down instantly and recovers exponentially
    //            (releaseMs); r' ≤ r always
    //   hold     h = sliding minimum of r' over W + 1 samples
    //   smooth   s = box average of h over W samples (W = attackMs)
    //
    // With the audio delayed by getLatency() = W − 1 + 8 samples, s(n) never
    // exceeds r' of the sample being output or its predecessor, so the output
    // sample peak never exceeds the ceiling and each inter-sample region is
    // covered by the gains on both of its sides. Linked: one gain for all
    // channels (no image shift). Below the ceiling the gain is exactly 1.
    class TruePeakLimiter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int phases = 8;
        static constexpr int tapsPerPhase = 48;
        static constexpr int detectLatency = tapsPerPhase / 2;
        static constexpr double attackMs = 1.5;
        static constexpr double releaseMs = 60.0;

        void prepare (double sampleRate);
        void reset() noexcept;

        void setCeilingDb (float db) noexcept;

        // Feeds one frame (numChannels samples at time n) and returns the gain
        // for the frame delayed by getLatency().
        float process (const float* frame, int numChannels) noexcept;

        int getLatency() const noexcept { return window - 1 + detectLatency; }
        int getWindow() const noexcept { return window; }

        // Interpolator taps, interleaved [tap][phase] (8 lanes, the last one
        // zero) so the seven fractional positions accumulate as one vector.
        const std::vector<float>& getInterpolator() const noexcept { return interp; }

    private:
        double fs = 48000.0;
        int window = 72;
        float ceiling = 0.891f;
        double releaseCoeff = 0.0;

        std::vector<float> interp;                // tapsPerPhase × 8 (interleaved phases)
        std::vector<float> history[maxChannels];  // doubled ring for contiguous reads
        int histPos = 0;
        float prevRegion = 0.0f;

        double released = 1.0;       // double: the slow recovery must reach exactly 1

        // Sliding minimum: monotonic deque in a power-of-two ring (head / tail
        // are running counters, masked on access).
        std::vector<float> dqValue;
        std::vector<long long> dqIndex;
        long long dqHead = 0, dqTail = 0, counter = 0;
        long long dqMask = 0;

        // Box average.
        std::vector<float> box;
        int boxPos = 0, belowUnity = 0;
        double boxSum = 0.0;
    };
}
