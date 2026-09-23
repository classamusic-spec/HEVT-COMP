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
    // original formulation. Integrated with one explicit step per sample: the
    // core always runs oversampled (≥ 88.2 kHz), where the per-sample change of
    // B is a tiny fraction of the loop width, so Euler and Heun agree to
    // within 0.01 dB in THD at half the cost. χ ≥ 0 and α·χ < 1 keep
    // dH/dB > 0 (stable, monotone branches).
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

        void setParams (const Params& p) noexcept
        {
            params = p;
            invA = 1.0 / p.a;
            invK = 1.0 / p.k;
            msOverA = p.ms / p.a;
            oneMinusAlpha = 1.0 - p.alpha;
        }
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
                m += slope (m, bPrev, db > 0.0 ? 1.0 : -1.0) * db;
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

        // Langevin function coth(x) − 1/x and its derivative. Near 0 a
        // series (4 terms, error < 1e-9 for |x| < 0.3); elsewhere one
        // single-precision exp (the slope only needs ~1e-6 relative accuracy:
        // the magnetisation itself is integrated in double).
        static void langevin (double x, double& value, double& derivative) noexcept
        {
            const double ax = std::abs (x);
            if (ax < 0.3)
            {
                const double x2 = x * x;
                value = x * (1.0 / 3.0 - x2 * (1.0 / 45.0 - x2 * (2.0 / 945.0 - x2 * (1.0 / 4725.0))));
                derivative = 1.0 / 3.0 - x2 * (1.0 / 15.0 - x2 * (2.0 / 189.0 - x2 * (7.0 / 4725.0)));
                return;
            }
            if (ax > 18.0)
            {
                value = (x > 0.0 ? 1.0 : -1.0) - 1.0 / x;
                derivative = 1.0 / (x * x);
                return;
            }
            const float xf = static_cast<float> (x);
            const float e2 = std::exp (2.0f * xf);
            const float invD = 1.0f / (e2 - 1.0f), invX = 1.0f / xf;
            value = static_cast<double> ((e2 + 1.0f) * invD - invX);               // coth(x) − 1/x
            derivative = static_cast<double> (invX * invX - 4.0f * e2 * invD * invD); // 1/x² − 1/sinh²(x)
        }

    private:
        double slope (double mCur, double b, double dir) const noexcept
        {
            const double he = b - oneMinusAlpha * mCur;
            double l = 0.0, dl = 0.0;
            langevin (he * invA, l, dl);
            const double man = params.ms * l;
            const double chi = params.c * msOverA * dl + std::max (0.0, dir * (man - mCur)) * invK;
            return chi / (1.0 + oneMinusAlpha * chi);
        }

        Params params;
        double invA = 1.0 / 0.03, invK = 1.0 / 0.03, msOverA = 1.0 / 0.03, oneMinusAlpha = 0.998;
        double bPrev = 0.0, m = 0.0, h = 0.0;
    };
}
