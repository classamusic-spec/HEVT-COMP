#include "TestFramework.h"

#include <chrono>
#include <cstring>

namespace heat::test
{
    int runAll (const char* filter)
    {
        auto& c = context();
        int testsRun = 0, testsFailed = 0;
        const auto start = std::chrono::steady_clock::now();

        for (auto& t : registry())
        {
            if (filter != nullptr && std::strstr (t.suite, filter) == nullptr && std::strstr (t.name, filter) == nullptr)
                continue;

            std::printf ("[ RUN  ] %s / %s\n", t.suite, t.name);
            const int before = c.failures;
            c.currentTest = t.name;
            t.body();
            ++testsRun;
            const bool ok = c.failures == before;
            if (! ok)
                ++testsFailed;
            std::printf ("[ %s ] %s / %s\n", ok ? " OK " : "FAIL", t.suite, t.name);
        }

        const auto ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();
        std::printf ("\n%d tests, %d checks, %d failed checks, %d failed tests (%.0f ms)\n",
                     testsRun, c.checks, c.failures, testsFailed, ms);
        return testsFailed == 0 ? 0 : 1;
    }
}

int main (int argc, char** argv)
{
    return heat::test::runAll (argc > 1 ? argv[1] : nullptr);
}
