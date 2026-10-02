#include "TestHarness.h"

#include <Hardware/SensorBoard.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Physics/SecondOrderFilter.h>
#include <ProcessControl.h>

#include <cmath>
#include <limits>

namespace
{
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

    // Échelon de 0 à 1 filtré pendant duration secondes, pas de dt.
    double stepResponse(double timeConstant, double duration, double dt)
    {
        SecondOrderFilter filter;
        filter.update(0.0, 0.0, timeConstant);

        double output = 0.0;

        for (double t = 0.0; t < duration - dt / 2; t += dt)
            output = filter.update(1.0, dt, timeConstant);

        return output;
    }

    void testSecondOrderResponse()
    {
        // H = 1 / (1 + τs)² : y(t) = 1 − e^(−t/τ) (1 + t/τ).
        for (double ratio : {0.5, 1.0, 2.0, 5.0})
        {
            const double expected =
                1.0 - std::exp(-ratio) * (1.0 + ratio);

            CHECK_NEAR(stepResponse(10.0, 10.0 * ratio, 0.01), expected, 0.002);
        }

        // La cadence ne change pas la réponse au premier ordre près.
        CHECK_NEAR(stepResponse(10.0, 10.0, 0.8), 0.2642, 0.03);
    }

    void testFilterEdgeCases()
    {
        SecondOrderFilter filter;
        CHECK_FALSE(filter.isStarted());

        // Première valeur reprise telle quelle.
        CHECK_NEAR(filter.update(42.0, 1.0, 10.0), 42.0, 0);
        CHECK_TRUE(filter.isStarted());

        // Pas de temps nul : rien ne bouge.
        CHECK_NEAR(filter.update(50.0, 0.0, 10.0), 42.0, 1e-12);

        // Constante nulle : pas de filtrage.
        CHECK_NEAR(filter.update(50.0, 1.0, 0.0), 50.0, 0);

        filter.reset();
        CHECK_FALSE(filter.isStarted());
        CHECK_NEAR(filter.update(7.0, 1.0, 10.0), 7.0, 0);
    }

    void testFilteredTemperature()
    {
        SensorBoard board;
        Sensor sensor("RTD", Sensor::Type::Pt100,
                      Sensor::Wiring::FourWire, 16, 0.0f);
        sensor.settings.filterTime = 10.0;

        Resistance resistance;
        resistance.begin("Resistance", board, sensor);
        TemperatureRTD temperature;
        temperature.begin("Temperature", resistance);

        ProcessControl process;
        CHECK_TRUE(process.add(resistance));
        CHECK_TRUE(process.add(temperature));

        // Première mesure : reprise telle quelle.
        board.resistanceOhms = 100.0;
        process.updateMeasurementsAndRegulators(0);
        CHECK_NEAR(temperature.getValue(), 0.0, 1e-6);

        // Échelon à 100 °C : la température filtrée monte doucement, la
        // résistance n'est pas filtrée.
        board.resistanceOhms = 138.506;
        uint32_t now = 0;
        for (int i = 0; i < 10; i++)
        {
            now += 1000;
            process.updateMeasurementsAndRegulators(now);
        }
        CHECK_NEAR(resistance.getValue(), 138.506, 1e-9);
        CHECK_NEAR(temperature.getValue(), 100.0 * 0.2642, 3.0);
        CHECK_TRUE(temperature.isValid());

        // Défaut : affiché immédiatement, sans valeur filtrée.
        board.resistanceOhms = NaN;
        sensor.add(32767);
        sensor.compute();
        process.updateMeasurementsAndRegulators(now += 1000);
        CHECK_TRUE(temperature.getStatus() == MeasurementStatus::Open);

        // Retour : le filtre repart de la nouvelle valeur.
        board.resistanceOhms = 138.506;
        process.updateMeasurementsAndRegulators(now += 1000);
        CHECK_TRUE(temperature.isValid());
        CHECK_NEAR(temperature.getValue(), 100.0, 0.005);

        // Filtre à 0 : valeur brute.
        sensor.settings.filterTime = 0.0;
        board.resistanceOhms = 100.0;
        process.updateMeasurementsAndRegulators(now += 1000);
        CHECK_NEAR(temperature.getValue(), 0.0, 1e-6);
    }
}

void runInputFilterTests()
{
    TestHarness::run("filtre d'entree : reponse du 2e ordre", testSecondOrderResponse);
    TestHarness::run("filtre d'entree : cas limites", testFilterEdgeCases);
    TestHarness::run("filtre d'entree : temperature RTD", testFilteredTemperature);
}
