#include "TestFramework.h"
#include "DSP/GainComputer.h"

using namespace heat::dsp;

HEAT_TEST ("GainComputer", "below threshold is unity")
{
    const GainComputerSettings s { -20.0f, 4.0f, 0.0f };
    for (float in = -120.0f; in <= -20.0f; in += 0.5f)
        CHECK_NEAR (GainComputer::gainReductionDb (in, s), 0.0, 1.0e-6);
}

HEAT_TEST ("GainComputer", "hard knee matches textbook formula")
{
    for (float ratio : { 1.5f, 2.0f, 4.0f, 8.0f, 20.0f })
        for (float thr : { -40.0f, -20.0f, -6.0f })
            for (float in = thr; in <= 12.0f; in += 1.0f)
            {
                const GainComputerSettings s { thr, ratio, 0.0f };
                const float expectedOut = thr + (in - thr) / ratio;
                CHECK_NEAR (GainComputer::outputDb (in, s), expectedOut, 1.0e-4);
                CHECK_NEAR (GainComputer::gainReductionDb (in, s), expectedOut - in, 1.0e-4);
            }
}

HEAT_TEST ("GainComputer", "ratio 1:1 never reduces gain")
{
    const GainComputerSettings s { -30.0f, 1.0f, 12.0f };
    for (float in = -60.0f; in <= 20.0f; in += 0.25f)
        CHECK_NEAR (GainComputer::gainReductionDb (in, s), 0.0, 1.0e-6);
}

HEAT_TEST ("GainComputer", "soft knee is continuous in value and slope")
{
    for (float knee : { 3.0f, 6.0f, 12.0f, 18.0f })
    {
        const GainComputerSettings s { -20.0f, 4.0f, knee };
        const float eps = 1.0e-3f;
        for (float edge : { -20.0f - knee * 0.5f, -20.0f + knee * 0.5f })
        {
            const float below = GainComputer::gainReductionDb (edge - eps, s);
            const float above = GainComputer::gainReductionDb (edge + eps, s);
            CHECK_NEAR (below, above, 2.0 * eps + 1.0e-5);

            const float slopeBelow = (GainComputer::gainReductionDb (edge - eps, s) - GainComputer::gainReductionDb (edge - 2 * eps, s)) / eps;
            const float slopeAbove = (GainComputer::gainReductionDb (edge + 2 * eps, s) - GainComputer::gainReductionDb (edge + eps, s)) / eps;
            CHECK_NEAR (slopeBelow, slopeAbove, 5.0e-3);
        }

        // At the threshold the soft knee applies exactly (slope * W/8).
        const float expected = (1.0f / 4.0f - 1.0f) * knee / 8.0f;
        CHECK_NEAR (GainComputer::gainReductionDb (-20.0f, s), expected, 1.0e-4);
    }
}

HEAT_TEST ("GainComputer", "static curve is monotonic (output never decreases)")
{
    for (float knee : { 0.0f, 6.0f, 15.0f })
        for (float ratio : { 1.2f, 3.0f, 20.0f })
        {
            const GainComputerSettings s { -24.0f, ratio, knee };
            float prev = -1.0e9f;
            bool ok = true;
            for (float in = -80.0f; in <= 24.0f; in += 0.05f)
            {
                const float out = GainComputer::outputDb (in, s);
                ok = ok && out >= prev - 1.0e-5f;
                prev = out;
            }
            CHECK (ok);
        }
}

HEAT_TEST ("GainComputer", "soft knee never gives less reduction than hard knee")
{
    for (float in = -40.0f; in <= 0.0f; in += 0.1f)
    {
        const float hard = GainComputer::gainReductionDb (in, { -20.0f, 4.0f, 0.0f });
        const float soft = GainComputer::gainReductionDb (in, { -20.0f, 4.0f, 10.0f });
        CHECK (soft <= hard + 1.0e-5f);
    }
}
