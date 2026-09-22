#pragma once

#include <algorithm>
#include <vector>

namespace heat::dsp
{
    // Fixed-capacity integer delay line (one channel). Storage is allocated in
    // prepare(); process calls never allocate.
    class DelayLine
    {
    public:
        void prepare (int maxDelaySamples)
        {
            buffer.assign (static_cast<size_t> (std::max (1, maxDelaySamples) + 1), 0.0f);
            writePos = 0;
            delay = std::min (delay, maxDelaySamples);
        }

        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

        void setDelay (int samples) noexcept
        {
            delay = std::clamp (samples, 0, static_cast<int> (buffer.size()) - 1);
        }

        int getDelay() const noexcept { return delay; }

        float process (float x) noexcept
        {
            const int size = static_cast<int> (buffer.size());
            buffer[static_cast<size_t> (writePos)] = x;
            int readPos = writePos - delay;
            if (readPos < 0)
                readPos += size;
            if (++writePos == size)
                writePos = 0;
            return buffer[static_cast<size_t> (readPos)];
        }

        void process (const float* in, float* out, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
                out[i] = process (in[i]);
        }

    private:
        std::vector<float> buffer { 0.0f };
        int writePos = 0;
        int delay = 0;
    };

    // Keeps the dry (parallel) path time-aligned with the latency of the wet
    // path so MIX never comb-filters.
    class DryWetAligner
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (int latencySamples)
        {
            for (auto& d : lines)
            {
                d.prepare (latencySamples);
                d.setDelay (latencySamples);
            }
        }

        void reset() noexcept
        {
            for (auto& d : lines)
                d.reset();
        }

        void process (int channel, const float* in, float* out, int n) noexcept
        {
            lines[channel].process (in, out, n);
        }

        int getLatency() const noexcept { return lines[0].getDelay(); }

    private:
        DelayLine lines[maxChannels];
    };
}
