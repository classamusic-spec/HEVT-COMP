#include "TestSignals.h"

#include <complex>

namespace heat::test
{
    namespace
    {
        void fft (std::vector<std::complex<double>>& a)
        {
            const size_t n = a.size();
            for (size_t i = 1, j = 0; i < n; ++i)
            {
                size_t bit = n >> 1;
                for (; j & bit; bit >>= 1)
                    j ^= bit;
                j ^= bit;
                if (i < j)
                    std::swap (a[i], a[j]);
            }
            for (size_t len = 2; len <= n; len <<= 1)
            {
                const double ang = -2.0 * dsp::pi / static_cast<double> (len);
                const std::complex<double> wl (std::cos (ang), std::sin (ang));
                for (size_t i = 0; i < n; i += len)
                {
                    std::complex<double> w (1.0);
                    for (size_t j = 0; j < len / 2; ++j)
                    {
                        const auto u = a[i + j];
                        const auto v = a[i + j + len / 2] * w;
                        a[i + j] = u + v;
                        a[i + j + len / 2] = u - v;
                        w *= wl;
                    }
                }
            }
        }
    }

    std::vector<double> spectrumDb (const float* x, int n)
    {
        std::vector<std::complex<double>> a (static_cast<size_t> (n));
        double wsum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = 2.0 * dsp::pi * i / (n - 1);
            const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
            a[static_cast<size_t> (i)] = x[i] * w;
            wsum += w;
        }
        fft (a);
        std::vector<double> mag (static_cast<size_t> (n / 2));
        for (int k = 0; k < n / 2; ++k)
            mag[static_cast<size_t> (k)] = 20.0 * std::log10 (std::max (1.0e-15, 2.0 * std::abs (a[static_cast<size_t> (k)]) / wsum));
        return mag;
    }

    double thd (const float* x, int n, double fs, double fundamentalHz, int maxHarmonic)
    {
        const double fundamental = toneAmplitude (x, n, fs, fundamentalHz);
        double sum = 0.0;
        for (int h = 2; h <= maxHarmonic; ++h)
        {
            const double f = fundamentalHz * h;
            if (f >= fs * 0.5 - 100.0)
                break;
            const double a = toneAmplitude (x, n, fs, f);
            sum += a * a;
        }
        return std::sqrt (sum) / std::max (1.0e-12, fundamental);
    }
}
