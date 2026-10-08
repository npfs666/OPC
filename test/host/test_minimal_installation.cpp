// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TemplateBench.h"

#include "../../examples/MinimalInstallation/MinimalInstallation.h"

#include <cstring>

/*
 * Exemple examples/MinimalInstallation : compilé et démarré ici pour qu'il
 * suive les évolutions d'OPC (il n'est pas dans src/).
 */
namespace
{
    // Ordre de process.add() dans l'exemple.
    constexpr size_t RESISTANCE = 0;
    constexpr size_t TEMPERATURE = 1;

    void testStartsAndMeasures()
    {
        TemplateBench<MinimalInstallation> bench;
        CHECK_TRUE(bench.started);
        CHECK_TRUE(
            std::strcmp(
                bench.installation.configurationKey(),
                "minimal_installation") == 0);

        bench.setTemperature(0, 21.5);
        bench.cycle();

        const MeasurementSample* temperature =
            bench.measurement(TEMPERATURE);
        CHECK_TRUE(temperature != nullptr);
        CHECK_TRUE(temperature != nullptr && temperature->valid);
        CHECK_NEAR(
            temperature != nullptr ? temperature->value : NAN,
            21.5,
            0.01);

        const MeasurementSample* resistance =
            bench.measurement(RESISTANCE);
        CHECK_TRUE(resistance != nullptr && resistance->valid);
    }

    void testSensorFault()
    {
        TemplateBench<MinimalInstallation> bench;
        bench.setTemperature(0, 21.5);
        bench.cycle();

        // Sonde en court-circuit : l'écran affichera l'état, pas la valeur.
        bench.shortSensor(0);
        bench.cycle();

        const MeasurementSample* temperature =
            bench.measurement(TEMPERATURE);
        CHECK_TRUE(temperature != nullptr && !temperature->valid);
        CHECK_TRUE(
            temperature != nullptr &&
            temperature->status == MeasurementStatus::Short);
    }

    void testParameters()
    {
        // Réglages de la sonde, plus ceux communs à toute installation.
        TemplateBench<MinimalInstallation> bench;
        CHECK_FALSE(bench.installation.getParameters().hasError());
        CHECK_TRUE(bench.parameter("menu", "inactivity_timeout") != nullptr);
    }
}

void runMinimalInstallationTests()
{
    TestHarness::run("exemple minimal : démarrage et mesure", testStartsAndMeasures);
    TestHarness::run("exemple minimal : défaut de sonde", testSensorFault);
    TestHarness::run("exemple minimal : paramètres", testParameters);
}
