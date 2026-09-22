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
        constexpr double modelFadeSeconds = 0.020;
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
        modelStep = static_cast<float> (1.0 / std::max (1.0, modelFadeSeconds * fs));

        coreMu = core.initialPermeability();

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
        core.reset();
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
        coreDrive = coreDriveFor (amount);
        coreDepth = coreDepthFor (amount);
    }

    void IronStage::setModel (IronModel model) noexcept
    {
        modelTarget = model == IronModel::hysteresis ? 1.0f : 0.0f;
    }

    void IronStage::setModelImmediate (IronModel model) noexcept
    {
        setModel (model);
        modelWeight = modelTarget;
    }

    float IronStage::process (float x) noexcept
    {
        // Flux (leaky integral of the signal).
        flux = flux * fluxLeak + static_cast<double> (x) * fluxGain;
        const float phi = static_cast<float> (flux);

        if (modelWeight < modelTarget)
        {
            if (modelWeight <= 0.0f)
            {
                // The core was idle: bring it to the current flux along the
                // initial magnetisation curve so the fade starts from a
                // physically consistent state.
                core.reset();
                const double target = coreDrive * flux;
                for (int i = 1; i <= 64; ++i)
                    core.process (target * i / 64.0);
            }
            modelWeight = std::min (modelTarget, modelWeight + modelStep);
        }
        else if (modelWeight > modelTarget)
            modelWeight = std::max (modelTarget, modelWeight - modelStep);

        float distortion = 0.0f;

        if (modelWeight < 1.0f)
        {
            // CLASSIC: direction-dependent offset + soft cubic core.
            const float dPhi = (phi - prevFlux) * slopeScale;
            const float phiH = phi - hc * dPhi / std::sqrt (1.0f + dPhi * dPhi);
            distortion += (1.0f - modelWeight) * depth * (-k * sat::softCubic (phiH));
        }
        prevFlux = phi;

        if (modelWeight > 0.0f)
        {
            // HYSTERESIS: magnetising current of a Jiles–Atherton core driven
            // by the flux, minus its linear (inductive) part.
            const double b = coreDrive * flux;
            const double h = core.process (b);
            const double e = (coreMu * h - b) / coreDrive;
            distortion += modelWeight * static_cast<float> (-coreDepth * e);
        }

        // Additive first-order low shelf (weight): no dip above the shelf and
        // a gentle extension into the low mids.
        shelfState += shelfCoeff * (x - shelfState);
        const float weighted = x + shelfGain * shelfState;

        const float y = weighted + dc.process (distortion);
        const float soft = hf.process (y);
        return y + hfBlend * (soft - y);
    }
}
