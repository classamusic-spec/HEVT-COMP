#pragma once

#include <vector>

namespace heat::dsp
{
    // Linear-phase polyphase half-band FIR (Kaiser-windowed sinc) for one 2x
    // stage. N = 4K + 3 taps; only the centre tap and the even taps are
    // non-zero, so:
    //   up:   even outputs = (2K+2)-tap FIR, odd outputs = pure delay of K
    //   down: (2K+2)-tap FIR on even inputs + 0.5 * delayed odd input
    // Round-trip latency is exactly N - 1 samples at the stage's high rate.
    class HalfbandStage
    {
    public:
        void design (int numTaps, double stopbandDb, int maxInputSamples);
        void reset() noexcept;

        // in: n samples at the low rate, out: 2n samples at the high rate.
        void upsample (const float* in, float* out, int n) noexcept;
        // in: 2n samples at the high rate, out: n samples at the low rate.
        void downsample (const float* in, float* out, int n) noexcept;

        int getNumTaps() const noexcept { return numTaps; }
        // Latency of one filter (up or down) in high-rate samples.
        int getFilterLatency() const noexcept { return (numTaps - 1) / 2; }

        const std::vector<float>& getPolyphaseCoefficients() const noexcept { return g; }

    private:
        int numTaps = 0, K = 0, L = 0; // L = number of polyphase taps (2K + 2)
        std::vector<float> g;          // polyphase (even-tap) coefficients, sum = 0.5

        // Up-sampler history (low rate), doubled for contiguous reads.
        std::vector<float> upHist;
        int upPos = 0;
        // Down-sampler histories: even-phase FIR input and odd-phase delay.
        std::vector<float> downHist, oddHist;
        int downPos = 0, oddPos = 0;
    };

    // Cascade of up to three 2x stages (2x / 4x / 8x). All stages for the
    // maximum factor are allocated in prepare(); the active stage count can be
    // changed at runtime without allocation. The oversampled round-trip is
    // padded at the high rate so its latency is an integer number of base-rate
    // samples.
    class Oversampler
    {
    public:
        static constexpr int maxStages = 3;
        static constexpr int maxChannels = 2;

        void prepare (int maxBlockSize);
        void reset() noexcept;

        void setNumStages (int stages) noexcept;
        int getNumStages() const noexcept { return numStages; }
        int getFactor() const noexcept { return 1 << numStages; }

        // Upsamples n base-rate samples of one channel into the internal
        // buffer; returns a pointer to n * factor high-rate samples which the
        // caller may process in place.
        float* upsample (int channel, const float* in, int n) noexcept;

        // Downsamples the internal buffer (after processing) to n base-rate
        // samples.
        void downsample (int channel, float* out, int n) noexcept;

        // Up-path latency in high-rate samples for the active stage count
        // (time from base-rate input to its appearance in the high-rate buffer).
        int getUpLatencyHighRate() const noexcept;
        // Full round-trip latency (base-rate samples) before padding, doubled
        // so it is an integer: returns 2 * latency.
        double getRoundTripLatencyBaseRate (int stages) const noexcept;

        const HalfbandStage& getStage (int channel, int index) const noexcept { return stages[channel][index]; }

        // Tap counts and stop-band attenuation per stage (index 0 = 2x).
        static constexpr int stageTaps[maxStages] = { 139, 31, 19 };
        static constexpr double stageStopbandDb[maxStages] = { 100.0, 100.0, 100.0 };

    private:
        HalfbandStage stages[maxChannels][maxStages];
        std::vector<float> work[maxChannels][maxStages + 1];
        int numStages = 1;
        int maxBlock = 0;
    };
}
