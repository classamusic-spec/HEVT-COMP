#pragma once

namespace heat::dsp
{
    // Static compression curve (feed-forward, log domain).
    //
    // Hard knee, above threshold:
    //     outputDb        = threshold + (inputDb - threshold) / ratio
    //     gainReductionDb = outputDb - inputDb
    //
    // Soft knee of width W dB centred on the threshold (quadratic
    // interpolation, continuous in value and first derivative):
    //     |over| <= W/2 :  gr = (1/ratio - 1) * (over + W/2)^2 / (2W)
    //
    // All gain values returned are <= 0 dB (reduction only).
    struct GainComputerSettings
    {
        float thresholdDb = 0.0f;
        float ratio = 1.0f;       // >= 1
        float kneeDb = 0.0f;      // >= 0
    };

    class GainComputer
    {
    public:
        // Returns gain reduction in dB (<= 0).
        static float gainReductionDb (float inputDb, const GainComputerSettings& s) noexcept;

        // Same curve expressed with a precomputed slope = (1/ratio - 1).
        static float gainReductionDb (float inputDb, float thresholdDb, float slope, float kneeDb) noexcept
        {
            const float over = inputDb - thresholdDb;
            const float halfKnee = 0.5f * kneeDb;

            if (over <= -halfKnee)
                return 0.0f;

            if (kneeDb > 0.0f && over < halfKnee)
            {
                const float x = over + halfKnee;
                return slope * x * x / (2.0f * kneeDb);
            }

            return slope * over;
        }

        // Static output level for a given input level.
        static float outputDb (float inputDb, const GainComputerSettings& s) noexcept
        {
            return inputDb + gainReductionDb (inputDb, s);
        }

        static float slopeForRatio (float ratio) noexcept
        {
            return 1.0f / (ratio < 1.0f ? 1.0f : ratio) - 1.0f;
        }
    };
}
