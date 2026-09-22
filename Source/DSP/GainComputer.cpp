#include "DSP/GainComputer.h"

#include <algorithm>

namespace heat::dsp
{
    float GainComputer::gainReductionDb (float inputDb, const GainComputerSettings& s) noexcept
    {
        return gainReductionDb (inputDb, s.thresholdDb, slopeForRatio (s.ratio), std::max (0.0f, s.kneeDb));
    }
}
