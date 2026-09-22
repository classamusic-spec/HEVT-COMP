#include "StageHarness.h"

namespace heat::test
{
    AliasReport measureAliasing (const float* x, int n, double fs, double f0)
    {
        const auto spec = spectrumDb (x, n);
        const double binHz = fs / n;
        const int f0Bin = static_cast<int> (std::lround (f0 / binHz));
        double fundamental = -300.0;
        for (int k = f0Bin - 3; k <= f0Bin + 3; ++k)
            fundamental = std::max (fundamental, spec[static_cast<size_t> (k)]);

        // Mark harmonic neighbourhoods (Blackman-Harris main lobe ≈ ±4 bins).
        std::vector<bool> masked (spec.size(), false);
        for (int h = 1; h * f0 < fs * 0.5; ++h)
        {
            const int b = static_cast<int> (std::lround (h * f0 / binHz));
            for (int k = b - 6; k <= b + 6; ++k)
                if (k >= 0 && k < static_cast<int> (masked.size()))
                    masked[static_cast<size_t> (k)] = true;
        }

        AliasReport r;
        const int lo = static_cast<int> (20.0 / binHz), hi = static_cast<int> (20000.0 / binHz);
        for (int k = std::max (8, lo); k <= std::min (hi, static_cast<int> (spec.size()) - 1); ++k)
        {
            if (masked[static_cast<size_t> (k)])
                continue;
            const double rel = spec[static_cast<size_t> (k)] - fundamental;
            if (rel > r.worstAliasDb)
            {
                r.worstAliasDb = rel;
                r.worstAliasHz = k * binHz;
            }
        }
        return r;
    }
}
