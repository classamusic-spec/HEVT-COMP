#include "Nonlinear/TubeStage.h"
#include "Nonlinear/SaturationUtilities.h"

namespace heat::dsp
{
    namespace
    {
        constexpr float biasShiftPerEnv = 0.30f;
        constexpr float envelopeMs = 40.0f;
        constexpr double hfSmoothingHz = 18000.0;
    }

    void TubeStage::prepare (double oversampledRate) noexcept
    {
        fs = oversampledRate;
        envCoeff = 1.0f - timeConstantCoeff (envelopeMs, fs);
        dc.prepare (fs);
        hf.setCutoff (hfSmoothingHz, fs);
        reset();
        setAmount (amount);
    }

    void TubeStage::reset() noexcept
    {
        env = 0.0f;
        dc.reset();
        hf.reset();
    }

    void TubeStage::setAmount (float a) noexcept
    {
        amount = clamp01 (a);
        // Drive: 0.35 (≈ -9 dB) → 5.5 (≈ +15 dB) into the transfer curve.
        drive = 0.35f * std::pow (10.0f, 1.2f * amount);
        bias = 0.06f + 0.24f * amount;
        depth = std::min (1.0f, amount / 0.15f);
        hfBlend = 0.6f * amount;
    }

    float TubeStage::process (float x) noexcept
    {
        const float v = drive * x;
        env += envCoeff * (std::abs (v) - env);
        const float b = std::max (-0.2f, bias - biasShiftPerEnv * env);

        const float shaped = (sat::asymmetric (v + b, 1.0f, negativeCeiling) - sat::asymmetric (b, 1.0f, negativeCeiling))
                             / (drive * sat::asymmetricSlope (b, 1.0f, negativeCeiling));

        const float distortion = depth * (shaped - x);
        const float y = x + dc.process (distortion);
        const float smoothed = hf.process (y);
        return y + hfBlend * (smoothed - y);
    }
}
