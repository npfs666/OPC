#include "TestHarness.h"

#include <Measurements/Temperature/TemperatureTC.h>
#include <Physics/Thermocouple.h>
#include <ProcessControl.h>
#include <Hardware/SensorBoard.h>
#include <ProcessSnapshot.h>

#include <cstring>
#include <limits>

namespace
{
    namespace Tc = Physics::Thermocouple;

    #include "thermocouple_reference.h"

    constexpr Tc::Type TYPES[] = {
        Tc::Type::B, Tc::Type::E, Tc::Type::J, Tc::Type::K,
        Tc::Type::N, Tc::Type::R, Tc::Type::S, Tc::Type::T
    };

    void testReferenceTables()
    {
        for (const auto& point : THERMOCOUPLE_REFERENCES)
        {
            CHECK_NEAR(
                Tc::temperatureToMillivolts(point.type, point.temperatureC),
                point.voltageMv,
                0.00051);

            // Aux très basses températures, l'arrondi à 1 µV des tables
            // représente beaucoup de °C. On teste ces bornes dans l'autre sens.
            const auto range = Tc::measurementRange(point.type);
            if (point.temperatureC <= range.minimum ||
                point.temperatureC >= range.maximum ||
                point.temperatureC < -100.0)
            {
                continue;
            }

            CHECK_NEAR(
                Tc::millivoltsToTemperature(point.type, point.voltageMv),
                point.temperatureC,
                0.2);
        }
    }

    void testRangesAndInversion()
    {
        for (const auto type : TYPES)
        {
            const auto reference = Tc::referenceRange(type);
            const auto measurement = Tc::measurementRange(type);
            CHECK_TRUE(std::isfinite(reference.minimum));
            CHECK_TRUE(reference.minimum <= measurement.minimum);
            CHECK_TRUE(std::strcmp(Tc::typeName(type), "?") != 0);
            CHECK_TRUE(std::isnan(Tc::temperatureToMillivolts(type, reference.minimum - 0.1)));
            CHECK_TRUE(std::isnan(Tc::temperatureToMillivolts(type, reference.maximum + 0.1)));

            for (int i = 0; i <= 200; i++)
            {
                const double temperature = i == 200 ? measurement.maximum :
                    measurement.minimum +
                    (measurement.maximum - measurement.minimum) * i / 200.0;
                const double voltage = Tc::temperatureToMillivolts(type, temperature);
                CHECK_NEAR(Tc::millivoltsToTemperature(type, voltage), temperature, 0.00001);
            }

            const double minimumMv = Tc::temperatureToMillivolts(type, measurement.minimum);
            const double maximumMv = Tc::temperatureToMillivolts(type, measurement.maximum);
            CHECK_TRUE(std::isnan(Tc::millivoltsToTemperature(type, minimumMv - 0.001)));
            CHECK_TRUE(std::isnan(Tc::millivoltsToTemperature(type, maximumMv + 0.001)));
        }

        // La fonction directe B reste utilisable à la jonction froide.
        CHECK_TRUE(std::isfinite(Tc::temperatureToMillivolts(Tc::Type::B, 20.0)));
        CHECK_TRUE(std::isnan(Tc::millivoltsToTemperature(Tc::Type::B, 0.0)));
        CHECK_NEAR(Tc::measurementRange(Tc::Type::B).minimum, 250.0, 0.0);
    }

    void testCompensation()
    {
        // Tensions issues des tables NIST, pas des fonctions testées.
        CHECK_NEAR(Tc::compensatedTemperature(Tc::Type::K, 4.096 - 0.798, 20.0), 100.0, 0.03);
        CHECK_NEAR(Tc::compensatedTemperature(Tc::Type::K, -3.554 - 0.798, 20.0), -100.0, 0.04);
        CHECK_NEAR(Tc::compensatedTemperature(Tc::Type::K, 4.096 - (-1.889), -50.0), 100.0, 0.04);
        CHECK_NEAR(Tc::compensatedTemperature(Tc::Type::E, 68.787 - 1.192, 20.0), 900.0, 0.03);

        for (const auto type : TYPES)
        {
            const auto range = Tc::measurementRange(type);
            const double hot = (range.minimum + range.maximum) * 0.5;
            for (const double cold : {0.0, 20.0, 50.0})
            {
                const double differential = Tc::temperatureToMillivolts(type, hot) -
                    Tc::temperatureToMillivolts(type, cold);
                CHECK_NEAR(Tc::compensatedTemperature(type, differential, cold), hot, 0.00001);
            }
        }
    }

    void testInvalidInputs()
    {
        const auto unknown = static_cast<Tc::Type>(255);
        CHECK_TRUE(std::isnan(Tc::referenceRange(unknown).minimum));
        CHECK_TRUE(std::isnan(Tc::measurementRange(unknown).maximum));
        CHECK_TRUE(std::strcmp(Tc::typeName(unknown), "?") == 0);
        CHECK_TRUE(std::isnan(Tc::temperatureToMillivolts(unknown, 20.0)));
        CHECK_TRUE(std::isnan(Tc::millivoltsToTemperature(unknown, 1.0)));
        CHECK_TRUE(std::isnan(Tc::compensatedTemperature(unknown, 1.0, 20.0)));

        for (const double invalid : {NAN, INFINITY, -INFINITY})
        {
            CHECK_TRUE(std::isnan(Tc::temperatureToMillivolts(Tc::Type::K, invalid)));
            CHECK_TRUE(std::isnan(Tc::millivoltsToTemperature(Tc::Type::K, invalid)));
            CHECK_TRUE(std::isnan(Tc::compensatedTemperature(Tc::Type::K, invalid, 20.0)));
            CHECK_TRUE(std::isnan(Tc::compensatedTemperature(Tc::Type::K, 0.0, invalid)));
        }
        CHECK_TRUE(std::isnan(Tc::compensatedTemperature(Tc::Type::K, 1000.0, 20.0)));
        CHECK_TRUE(std::isnan(Tc::compensatedTemperature(Tc::Type::K, 0.0, -271.0)));
    }

    void testMeasurement()
    {
        SensorBoard board;
        Sensor sensor;
        sensor.begin("input", Sensor::Type::Tc, Sensor::Wiring::TwoWire, 16, 0);

        TemperatureTC temperature;
        temperature.update();
        CHECK_FALSE(temperature.isValid());

        temperature.begin("TC", sensor);
        temperature.update();
        CHECK_FALSE(temperature.isValid());
        CHECK_TRUE(std::isnan(temperature.getValue()));

        CHECK_TRUE(board.addSensor(sensor));
        board.voltageMv = 3.298;
        board.adcTemperature = 22.24;
        board.settings.coldJunctionOffset = -2.24;
        temperature.update();
        CHECK_TRUE(temperature.isValid());
        CHECK_TRUE(std::strcmp(temperature.getUnit(), "°C") == 0);
        CHECK_NEAR(temperature.getValue(), 100.0, 0.03);

        sensor.settings.offset = 1.5;
        temperature.update();
        CHECK_NEAR(temperature.getValue(), 101.5, 0.03);
        sensor.settings.offset = 0;

        // Le même offset corrige la jonction froide avant la conversion non linéaire.
        board.voltageMv = 50.644 - 0.798;
        temperature.update();
        CHECK_NEAR(temperature.getValue(), 1250.0, 0.04);

        board.voltageMv = NAN; // erreur d'acquisition ou saturation signalée par la source
        temperature.update();
        CHECK_FALSE(temperature.isValid());
        CHECK_TRUE(std::isnan(temperature.getValue()));

        sensor.settings.thermocoupleType = Tc::Type::T;
        board.voltageMv = 4.279 - 0.790;
        board.adcTemperature = 20;
        board.settings.coldJunctionOffset = 0;
        temperature.update();
        CHECK_TRUE(temperature.isValid());
        CHECK_NEAR(temperature.getValue(), 100.0, 0.03);

        for (const double invalid : {NAN, INFINITY, -INFINITY})
        {
            board.settings.coldJunctionOffset = invalid;
            temperature.update();
            CHECK_FALSE(temperature.isValid());
        }
        board.settings.coldJunctionOffset = 0.0;
        board.voltageMv = 100.0;
        temperature.update();
        CHECK_FALSE(temperature.isValid());

        board.voltageMv = 0.0;
        board.adcTemperature = NAN;
        temperature.update();
        CHECK_FALSE(temperature.isValid());

        sensor.settings.thermocoupleType = Tc::Type::K;
        board.voltageMv = 0;
        board.adcTemperature = 20;
        board.settings.coldJunctionOffset = 0;
        temperature.begin("TC reinitialisee", sensor);
        CHECK_FALSE(temperature.isValid());
        temperature.update();
        CHECK_TRUE(temperature.isValid());
        CHECK_NEAR(temperature.getValue(), 20.0, 0.00001);
    }

    void testSensorAcquisition()
    {
        Sensor sensor;
        sensor.begin("TC", Sensor::Type::Tc, Sensor::Wiring::TwoWire, 2, 0);
        CHECK_TRUE(std::isnan(sensor.readValue()));
        sensor.add(100);
        sensor.add(200);
        sensor.compute();
        CHECK_NEAR(sensor.readValue(), 150, 0);
        sensor.addLP(32767);
        sensor.addLP(100);
        sensor.compute();
        CHECK_TRUE(std::isnan(sensor.readValue()));
        sensor.add(-32768);
        sensor.add(0);
        sensor.compute();
        CHECK_TRUE(std::isnan(sensor.readValue()));
        sensor.add(50);
        sensor.add(70);
        sensor.compute();
        CHECK_NEAR(sensor.readValue(), 60, 0);
        sensor.reset();
        CHECK_TRUE(std::isnan(sensor.readValue()));
    }

    void testProcessIntegration()
    {
        SensorBoard board;
        Sensor sensor;
        sensor.begin("input", Sensor::Type::Tc, Sensor::Wiring::TwoWire, 16, 0);
        CHECK_TRUE(board.addSensor(sensor));
        board.voltageMv = 3.298;
        board.adcTemperature = 20;
        TemperatureTC temperature;
        temperature.begin("TC", sensor);
        ProcessControl process;
        CHECK_TRUE(process.add(temperature));

        process.updateMeasurementsAndRegulators(1000);
        ProcessSnapshot snapshot;
        process.captureSnapshot(snapshot, 1000);
        const auto* captured = snapshot.find(temperature);
        CHECK_TRUE(captured != nullptr);
        if (captured != nullptr)
        {
            CHECK_TRUE(captured->valid);
            CHECK_NEAR(captured->value, 100.0, 0.03);
        }

        board.voltageMv = NAN;
        process.updateMeasurementsAndRegulators(2000);
        process.captureSnapshot(snapshot, 2000);
        captured = snapshot.find(temperature);
        CHECK_TRUE(captured != nullptr);
        if (captured != nullptr)
            CHECK_FALSE(captured->valid);
    }
}

void runThermocoupleTests()
{
    TestHarness::run("thermocouples : tables NIST", testReferenceTables);
    TestHarness::run("thermocouples : domaines et inversion", testRangesAndInversion);
    TestHarness::run("thermocouples : jonction froide", testCompensation);
    TestHarness::run("thermocouples : entrees invalides", testInvalidInputs);
    TestHarness::run("TemperatureTC : offset et validite", testMeasurement);
    TestHarness::run("Sensor : acquisition et saturation", testSensorAcquisition);
    TestHarness::run("TemperatureTC : processus et snapshot", testProcessIntegration);
}
