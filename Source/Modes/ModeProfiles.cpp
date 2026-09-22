#include "Modes/ModeProfiles.h"

#include <algorithm>
#include <cmath>

namespace heat::dsp
{
    float MacroCurve::evaluate (float compress) const noexcept
    {
        // Monotone cubic Hermite interpolation over 5 equally spaced anchors.
        constexpr int n = 5;
        constexpr float h = 0.25f;
        const float x = std::clamp (compress, 0.0f, 1.0f);

        float delta[n - 1];
        for (int i = 0; i < n - 1; ++i)
            delta[i] = (anchors[i + 1] - anchors[i]) / h;

        float m[n];
        m[0] = delta[0];
        m[n - 1] = delta[n - 2];
        for (int i = 1; i < n - 1; ++i)
            m[i] = (delta[i - 1] * delta[i] <= 0.0f) ? 0.0f : 0.5f * (delta[i - 1] + delta[i]);

        // Fritsch–Carlson limiter guarantees monotonicity.
        for (int i = 0; i < n - 1; ++i)
        {
            if (delta[i] == 0.0f)
            {
                m[i] = m[i + 1] = 0.0f;
                continue;
            }
            const float a = m[i] / delta[i];
            const float b = m[i + 1] / delta[i];
            const float s = a * a + b * b;
            if (s > 9.0f)
            {
                const float t = 3.0f / std::sqrt (s);
                m[i] = t * a * delta[i];
                m[i + 1] = t * b * delta[i];
            }
        }

        const int seg = std::min (n - 2, static_cast<int> (x / h));
        const float t = (x - static_cast<float> (seg) * h) / h;
        const float t2 = t * t, t3 = t2 * t;
        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;
        return h00 * anchors[seg] + h10 * h * m[seg] + h01 * anchors[seg + 1] + h11 * h * m[seg + 1];
    }

    namespace
    {
        // Anchor tables were tuned with the macro sweep in Tests/CompressMacroTests.cpp
        // and tools/heat_measure (see docs/COMPRESS_MACRO.md for the resulting
        // GR tables).
        const ModeProfile cleanProfile {
            "CLEAN",
            /* threshold */ { { -6.0f, -16.5f, -21.0f, -26.5f, -35.0f } },
            /* ratio     */ { {  1.0f,   1.5f,   2.5f,   4.5f,   8.0f } },
            /* knee      */ { {  6.0f,   6.0f,   6.0f,   6.0f,   6.0f } },
            /* attackScale */ 1.0f, /* releaseScale */ 1.0f,
            /* rounding */ 0.0f, 0.0f,
            /* memory */ 0.0f, 0.5f, 500.0f, 4.0f,
            /* detectorSoftening */ 0.0f,
            /* makeupFactor */ 0.6f,
            /* colorDrive */ { { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
            /* colorGrInteraction */ 0.0f,
            /* colorBias */ 0.0f
        };

        const ModeProfile warmProfile {
            "WARM",
            /* threshold */ { { -8.0f, -18.5f, -22.5f, -28.5f, -36.0f } },
            /* ratio     */ { {  1.0f,   1.4f,   2.0f,   3.0f,   4.5f } },
            /* knee      */ { { 12.0f,  12.0f,  13.0f,  14.0f,  15.0f } },
            /* attackScale */ 1.3f, /* releaseScale */ 1.15f,
            /* rounding */ 0.35f, 0.4f,
            /* memory */ 0.75f, 0.55f, 600.0f, 5.0f,
            /* detectorSoftening */ 0.3f,
            /* makeupFactor */ 0.65f,
            /* colorDrive */ { { 0.10f, 0.14f, 0.20f, 0.28f, 0.36f } },
            /* colorGrInteraction */ 0.01f,
            /* colorBias */ 0.25f
        };

        const ModeProfile driveProfile {
            "DRIVE",
            /* threshold */ { { -4.0f, -15.5f, -21.0f, -28.0f, -38.0f } },
            /* ratio     */ { {  1.0f,   2.0f,   4.0f,   8.0f,  20.0f } },
            /* knee      */ { {  3.0f,   3.0f,   3.0f,   3.0f,   3.0f } },
            /* attackScale */ 0.5f, /* releaseScale */ 0.6f,
            /* rounding */ 0.0f, 0.0f,
            /* memory */ 0.0f, 0.4f, 300.0f, 3.0f,
            /* detectorSoftening */ 0.0f,
            /* makeupFactor */ 0.7f,
            /* colorDrive */ { { 0.25f, 0.34f, 0.45f, 0.58f, 0.72f } },
            /* colorGrInteraction */ 0.025f,
            /* colorBias */ 0.12f
        };

        //                                    name       atkScale floor relScale boost  memW  depth charge relMult
        const DetectorProfile peakProfile    { "PEAK",    1.0f,   0.0f, 1.0f,    0.0f,  0.0f, 0.5f, 500.0f, 4.0f };
        const DetectorProfile rmsProfile     { "RMS",     1.0f,   0.0f, 1.0f,    0.0f,  0.0f, 0.5f, 500.0f, 4.0f };
        const DetectorProfile opticalProfile { "OPTICAL", 2.5f,   4.0f, 0.6f,    0.12f, 1.0f, 0.6f, 350.0f, 8.0f };
    }

    const ModeProfile& getModeProfile (Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::clean: return cleanProfile;
            case Mode::drive: return driveProfile;
            case Mode::warm:
            default:          return warmProfile;
        }
    }

    const DetectorProfile& getDetectorProfile (Detector detector) noexcept
    {
        switch (detector)
        {
            case Detector::rms:     return rmsProfile;
            case Detector::optical: return opticalProfile;
            case Detector::peak:
            default:                return peakProfile;
        }
    }
}
