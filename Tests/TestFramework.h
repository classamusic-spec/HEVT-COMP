#pragma once

// Minimal self-contained test harness (no external dependencies so the DSP
// tests build anywhere, including sanitizer builds).

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace heat::test
{
    struct TestCase
    {
        const char* suite;
        const char* name;
        std::function<void()> body;
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    struct Registrar
    {
        Registrar (const char* suite, const char* name, std::function<void()> body)
        {
            registry().push_back ({ suite, name, std::move (body) });
        }
    };

    struct Context
    {
        int failures = 0;
        int checks = 0;
        const char* currentTest = "";
    };

    inline Context& context()
    {
        static Context c;
        return c;
    }

    inline void report (bool ok, const char* expr, const char* file, int line, const std::string& detail = {})
    {
        auto& c = context();
        ++c.checks;
        if (! ok)
        {
            ++c.failures;
            std::printf ("    FAIL %s:%d  %s %s\n", file, line, expr, detail.c_str());
        }
    }

    // Records a named measurement so every test run leaves an audit trail.
    inline void note (const char* fmt, double a = 0, double b = 0, double c = 0, double d = 0)
    {
        std::printf ("      · ");
        std::printf (fmt, a, b, c, d);
        std::printf ("\n");
    }

    int runAll (const char* filter);
}

#define HEAT_CONCAT_INNER(a, b) a##b
#define HEAT_CONCAT(a, b) HEAT_CONCAT_INNER (a, b)

#define HEAT_TEST(suite, name)                                                              \
    static void HEAT_CONCAT (heatTest_, __LINE__)();                                        \
    static heat::test::Registrar HEAT_CONCAT (heatReg_, __LINE__) (suite, name,             \
                                                                  HEAT_CONCAT (heatTest_, __LINE__)); \
    static void HEAT_CONCAT (heatTest_, __LINE__)()

#define CHECK(expr) heat::test::report ((expr), #expr, __FILE__, __LINE__)

#define CHECK_NEAR(a, b, tol)                                                               \
    do {                                                                                    \
        const double heat_a_ = (double) (a), heat_b_ = (double) (b);                        \
        char heat_buf_[160];                                                                \
        std::snprintf (heat_buf_, sizeof (heat_buf_), "(%g vs %g, tol %g)", heat_a_, heat_b_, (double) (tol)); \
        heat::test::report (std::abs (heat_a_ - heat_b_) <= (double) (tol),                 \
                            #a " ~= " #b, __FILE__, __LINE__, heat_buf_);                   \
    } while (false)

#define CHECK_RANGE(v, lo, hi)                                                              \
    do {                                                                                    \
        const double heat_v_ = (double) (v);                                                \
        char heat_buf_[160];                                                                \
        std::snprintf (heat_buf_, sizeof (heat_buf_), "(%g not in [%g, %g])", heat_v_, (double) (lo), (double) (hi)); \
        heat::test::report (heat_v_ >= (double) (lo) && heat_v_ <= (double) (hi),           \
                            #v " in range", __FILE__, __LINE__, heat_buf_);                 \
    } while (false)
