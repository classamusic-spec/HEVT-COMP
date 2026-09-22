#include "Nonlinear/TubeStage.h"
#include "Nonlinear/SaturationUtilities.h"

namespace heat::dsp
{
    namespace
    {
        constexpr float biasShiftPerEnv = 0.25f;
        constexpr float envelopeMs = 40.0f;
        constexpr double hfSmoothingHz = 18000.0;
        constexpr float driveMin = 0.10f, driveMax = 2.4f;
        constexpr float biasBase = 0.04f, biasPerAmount = 0.26f;

        // Smooth lower bound (keeps the operating point analytic in env).
        inline float softFloor (float x, float floorValue) noexcept
        {
            const float d = x - floorValue;
            return floorValue + 0.5f * (d + std::sqrt (d * d + 0.0025f));
        }
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
        // Drive: 0.10 (-20 dB) → 2.4 (+7.6 dB) into the transfer curve, log-spaced.
        drive = driveMin * std::pow (driveMax / driveMin, amount);
        bias = biasBase + biasPerAmount * amount;
        depth = std::min (1.0f, amount / 0.15f);
        hfBlend = 0.6f * amount;
    }

    float TubeStage::process (float x) noexcept
    {
        const float v = drive * x;
        env += envCoeff * (std::abs (v) - env);
        const float b = softFloor (bias - biasShiftPerEnv * env, -0.1f);

        // Analytic transfer curve (no kinks at any derivative order, so the
        // harmonic series decays fast and oversampling is effective). The
        // asymmetry comes from the operating point b alone.
        const float shaped = (sat::algebraic (v + b, 1.0f) - sat::algebraic (b, 1.0f))
                             / (drive * sat::algebraicSlope (b, 1.0f));

        const float distortion = depth * (shaped - x);
        const float y = x + dc.process (distortion);
        const float smoothed = hf.process (y);
        return y + hfBlend * (smoothed - y);
    }
}
