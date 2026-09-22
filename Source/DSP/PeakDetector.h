#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // PEAK: instantaneous rectified level. Timing is applied afterwards, on the
    // gain-reduction signal (log-domain branching ballistics), so the detector
    // itself adds no smoothing and reacts to the very first sample of a
    // transient.
    class PeakDetector
    {
    public:
        void reset() noexcept {}

        float processDb (float x) noexcept
        {
            return gainToDb (std::abs (x));
        }
    };
}
