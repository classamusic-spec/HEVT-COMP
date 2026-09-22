#include "Nonlinear/IronStage.h"
#include "Nonlinear/SaturationUtilities.h"

namespace heat::dsp
{
    namespace
    {
        constexpr double fluxReferenceHz = 50.0;
        constexpr double fluxLeakHz = 8.0;
        constexpr double shelfHz = 80.0;
        constexpr double hfSoftHz = 24000.0;
        constexpr float maxShelfDb = 1.6f;
    }

    void IronStage::prepare (double oversampledRate) noexcept
    {
        fs = oversampledRate;
        fluxLeak = std::exp (-2.0 * pi * fluxLeakHz / fs);
        fluxGain = 2.0 * pi * fluxReferenceHz / fs;
        // Scale for the flux-slope sigmoid: a 50 Hz full-scale sine has a peak
        // per-sample flux change of 2π·50/fs.
        slopeScale = static_cast<float> (fs / (2.0 * pi * fluxReferenceHz)) * 4.0f;

        shelfCoeff = static_cast<float> (-std::expm1 (-2.0 * pi * shelfHz / fs));

        dc.prepare (fs);
        hf.setCutoff (hfSoftHz, fs);
        reset();
        setAmount (amount);
    }

    void IronStage::reset() noexcept
    {
        flux = 0.0;
        prevFlux = 0.0f;
        shelfState = 0.0f;
        dc.reset();
        hf.reset();
    }

    void IronStage::setAmount (float a) noexcept
    {
        amount = clamp01 (a);
        depth = std::min (1.0f, amount / 0.15f);
        k = 0.50f * amount * amount + 0.15f * amount;
        hc = 0.04f * amount;
        shelfGain = dbToGain (maxShelfDb * amount) - 1.0f;
        hfBlend = 0.5f * amount;
    }

    float IronStage::process (float x) noexcept
    {
        // Flux (leaky integral of the signal).
        flux = flux * fluxLeak + static_cast<double> (x) * fluxGain;
        const float phi = static_cast<float> (flux);
        const float dPhi = (phi - prevFlux) * slopeScale;
        prevFlux = phi;

        // Direction-dependent offset (hysteresis-like memory).
        const float phiH = phi - hc * dPhi / std::sqrt (1.0f + dPhi * dPhi);

        // Core saturation → low-frequency weighted odd harmonics.
        const float distortion = depth * (-k * sat::softCubic (phiH));

        // Additive first-order low shelf (weight): no dip above the shelf and
        // a gentle extension into the low mids.
        shelfState += shelfCoeff * (x - shelfState);
        const float weighted = x + shelfGain * shelfState;

        const float y = weighted + dc.process (distortion);
        const float soft = hf.process (y);
        return y + hfBlend * (soft - y);
    }
}
