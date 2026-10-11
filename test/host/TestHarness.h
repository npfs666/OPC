// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OPC_TEST_HARNESS_H
#define OPC_TEST_HARNESS_H

// Tests hôte sur doctest (doctest/doctest.h, licence MIT) : chaque
// TEST_CASE est découvert par l'exécutable et par C++ TestMate.

#include <cmath>

#include "doctest/doctest.h"

namespace TestHarness
{
    inline bool isNear(
        double actual,
        double expected,
        double tolerance)
    {
        return std::isfinite(actual) &&
               std::fabs(actual - expected) <=
                   tolerance;
    }
}

// Le cast évite la décomposition doctest, qui refuse && et ||.
#define CHECK_TRUE(expression) \
    DOCTEST_CHECK(static_cast<bool>(expression))

// Remplace le CHECK_FALSE de doctest, pour la même raison.
#undef CHECK_FALSE
#define CHECK_FALSE(expression) \
    DOCTEST_CHECK_FALSE(static_cast<bool>(expression))

#define CHECK_NEAR(actual, expected, tolerance) \
    do \
    { \
        const double opcActual = static_cast<double>(actual); \
        const double opcExpected = static_cast<double>(expected); \
        const double opcTolerance = static_cast<double>(tolerance); \
        DOCTEST_CHECK_MESSAGE( \
            TestHarness::isNear( \
                opcActual, \
                opcExpected, \
                opcTolerance), \
            #actual " ~= " #expected " : " \
                << opcActual << " / " << opcExpected \
                << " ± " << opcTolerance); \
    } while (false)

#endif
