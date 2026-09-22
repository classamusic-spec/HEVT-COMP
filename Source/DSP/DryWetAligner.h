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

    // Delay line whose length can change at runtime without a click: a new
    // length is reached by cross-fading from the old read tap to the new one
    // over `fadeSamples` with a smoothstep curve (zero slope at both ends, so
    // the fade itself adds no corner to the waveform). A request made during a fade is taken up as
    // soon as that fade completes. Used where HEAT's latency changes
    // (LOOKAHEAD, LIMITER) so the bypass, listen and audio paths move together.
    class CrossfadeDelay
    {
    public:
        void prepare (int maxDelaySamples, int fadeSamples)
        {
            buffer.assign (static_cast<size_t> (std::max (1, maxDelaySamples) + 1), 0.0f);
            fadeLength = std::max (1, fadeSamples);
            writePos = 0;
            current = target = next = std::min (current, maxDelaySamples);
            fading = false;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            current = next = target;
            fading = false;
        }

        // Jumps to a length without a fade (prepare / reset only).
        void snapDelay (int samples) noexcept
        {
            target = clampDelay (samples);
            current = next = target;
            fading = false;
        }

        void setDelay (int samples) noexcept { target = clampDelay (samples); }

        int getTargetDelay() const noexcept { return target; }
        int getCurrentDelay() const noexcept { return current; }
        bool isFading() const noexcept { return fading || target != current; }

        float process (float x) noexcept
        {
            const int size = static_cast<int> (buffer.size());
            buffer[static_cast<size_t> (writePos)] = x;

            if (! fading && target != current)
            {
                next = target;
                fadePos = 0;
                fading = true;
            }

            float y = read (current, size);
            if (fading)
            {
                const float u = static_cast<float> (fadePos + 1) / static_cast<float> (fadeLength);
                const float t = u * u * (3.0f - 2.0f * u);
                y += t * (read (next, size) - y);
                if (++fadePos >= fadeLength)
                {
                    current = next;
                    fading = false;
                }
            }

            if (++writePos == size)
                writePos = 0;
            return y;
        }

    private:
        int clampDelay (int samples) const noexcept
        {
            return std::clamp (samples, 0, static_cast<int> (buffer.size()) - 1);
        }

        float read (int delay, int size) const noexcept
        {
            int readPos = writePos - delay;
            if (readPos < 0)
                readPos += size;
            return buffer[static_cast<size_t> (readPos)];
        }

        std::vector<float> buffer { 0.0f };
        int writePos = 0;
        int current = 0, next = 0, target = 0;
        int fadeLength = 1, fadePos = 0;
        bool fading = false;
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
