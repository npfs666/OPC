#ifndef OPC_TEST_HARNESS_H
#define OPC_TEST_HARNESS_H

#include <cmath>
#include <iostream>
#include <string>

namespace TestHarness
{
    inline size_t executed = 0;
    inline size_t failed = 0;

    inline void check(
        bool condition,
        const char* expression,
        const char* file,
        int line)
    {
        if (condition)
            return;

        failed++;
        std::cerr
            << file << ':' << line
            << ": échec: "
            << expression << '\n';
    }

    inline void checkNear(
        double actual,
        double expected,
        double tolerance,
        const char* expression,
        const char* file,
        int line)
    {
        check(
            std::isfinite(actual) &&
                std::fabs(actual - expected) <=
                    tolerance,
            expression,
            file,
            line);
    }

    inline void run(
        const char* name,
        void (*test)())
    {
        const size_t failuresBefore = failed;
        executed++;

        test();

        std::cout
            << (failed == failuresBefore
                    ? "[OK]   "
                    : "[FAIL] ")
            << name << '\n';
    }

    inline int finish()
    {
        std::cout
            << executed << " test(s), "
            << failed << " échec(s)\n";

        return failed == 0 ? 0 : 1;
    }
}

#define CHECK_TRUE(expression) \
    TestHarness::check( \
        static_cast<bool>(expression), \
        #expression, \
        __FILE__, \
        __LINE__)

#define CHECK_FALSE(expression) \
    CHECK_TRUE(!(expression))

#define CHECK_NEAR(actual, expected, tolerance) \
    TestHarness::checkNear( \
        static_cast<double>(actual), \
        static_cast<double>(expected), \
        static_cast<double>(tolerance), \
        #actual " ~= " #expected, \
        __FILE__, \
        __LINE__)

#endif
