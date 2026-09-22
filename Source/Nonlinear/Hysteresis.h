#pragma once

#include <algorithm>
#include <cmath>

namespace heat::dsp
{
    // Inverse Jiles–Atherton ferromagnetic core (normalised units, μ0 = 1).
    //
    // A transformer driven from a voltage source sets the core's flux density
    // B (B ∝ ∫v dt); the magnetising field H that the source must supply is a
    // nonlinear, history-dependent function of B. The inverse J–A model
    // (Sadowski et al., IEEE Trans. Magn. 2002) integrates the magnetisation M
    // along the B trajectory:
    //
    //   He   = H + αM = B − (1 − α) M              effective field
    //   Man  = Ms · L(He / a)                      anhysteretic curve, L = Langevin
    //   χ    = c · dMan/dHe + max(0, δ (Man − M)) / k
    //   dM/dB = χ / (1 + (1 − α) χ),   H = B − M
    //
    // δ = sign(dB). The max(0, ·) term is the irreversible (domain-wall
    // pinning) contribution written with M_irr eliminated through
    // M = c·Man + (1 − c)·M_irr; it only ever pulls M towards the anhysteretic
    // curve, which removes the non-physical negative susceptibility of the
    // original formulation. Integrated with Heun's method per sample; χ ≥ 0
    // and α·χ < 1 keep dH/dB > 0 (stable, monotone branches).
    class JilesAthertonCore
    {
    public:
        struct Params
        {
            double ms = 1.0;      // saturation magnetisation
            double a = 0.03;      // anhysteretic shape (field scale of saturation)
            double k = 0.03;      // pinning (loop half-width in field units)
            double c = 0.4;       // reversibility 0..1
            double alpha = 0.002; // inter-domain coupling
        };

        void setParams (const Params& p) noexcept { params = p; }
        const Params& getParams() const noexcept { return params; }

        // Demagnetised state.
        void reset() noexcept
        {
            bPrev = 0.0;
            m = 0.0;
            h = 0.0;
        }

        // Advances the core to flux density b and returns the field H.
        double process (double b) noexcept
        {
            const double db = b - bPrev;
            if (std::abs (db) > 0.0)
            {
                const double dir = db > 0.0 ? 1.0 : -1.0;
                const double s1 = slope (m, bPrev, dir);
                const double mPred = m + s1 * db;
                const double s2 = slope (mPred, b, dir);
                m += 0.5 * (s1 + s2) * db;
                bPrev = b;
            }
            h = b - m;
            return h;
        }

        double getM() const noexcept { return m; }
        double getH() const noexcept { return h; }

        // Small-signal relative permeability of the virgin core (dB/dH at 0).
        double initialPermeability() const noexcept
        {
            const double chi = params.c * params.ms / (3.0 * params.a);
            const double dmdb = chi / (1.0 + (1.0 - params.alpha) * chi);
            return 1.0 / (1.0 - dmdb);
        }

        // Langevin function coth(x) − 1/x and its derivative, from a single exp.
        static void langevin (double x, double& value, double& derivative) noexcept
        {
            const double ax = std::abs (x);
            if (ax < 1.0e-2)
            {
                const double x2 = x * x;
                value = x * (1.0 / 3.0 - x2 * (1.0 / 45.0 - x2 * (2.0 / 945.0)));
                derivative = 1.0 / 3.0 - x2 * (1.0 / 15.0 - x2 * (2.0 / 189.0));
                return;
            }
            if (ax > 18.0)
            {
                value = (x > 0.0 ? 1.0 : -1.0) - 1.0 / x;
                derivative = 1.0 / (x * x);
                return;
            }
            const double d = std::expm1 (2.0 * x);
            const double e2 = d + 1.0;
            value = (e2 + 1.0) / d - 1.0 / x;              // coth(x) − 1/x
            derivative = 1.0 / (x * x) - 4.0 * e2 / (d * d); // 1/x² − 1/sinh²(x)
        }

    private:
        double slope (double mCur, double b, double dir) const noexcept
        {
            const double he = b - (1.0 - params.alpha) * mCur;
            const double x = he / params.a;
            double l = 0.0, dl = 0.0;
            langevin (x, l, dl);
            const double man = params.ms * l;
            const double chiAn = params.ms / params.a * dl;
            const double chi = params.c * chiAn + std::max (0.0, dir * (man - mCur)) / params.k;
            return chi / (1.0 + (1.0 - params.alpha) * chi);
        }

        Params params;
        double bPrev = 0.0, m = 0.0, h = 0.0;
    };
}
