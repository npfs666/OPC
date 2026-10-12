// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Measurements/Humidity/HumidityPsychrometer.h>
#include <Measurements/Pressure/Pressure.h>
#include <Measurements/Psychrometer.h>
#include <Measurements/Temperature/Temperature.h>
#include <Physics/Psychrometrics.h>
#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    using Physics::Psychrometrics;

    constexpr double_t SEA_LEVEL = 101325.0;

    class FakeTemperature final : public Temperature
    {
    public:
        explicit FakeTemperature(const char* name)
        {
            Temperature::begin(name);
        }

        void set(double_t value)
        {
            setValue(value);
            setValid(true);
        }

        void fail(MeasurementStatus status)
        {
            setStatus(status);
        }

        void update() override {}
    };

    class FakePressure final : public Pressure
    {
    public:
        FakePressure()
        {
            Pressure::begin("pression");
        }

        void set(double_t value)
        {
            setValue(value);
            setValid(true);
        }

        void update() override {}
    };

    struct Bench
    {
        FakeTemperature dry{"seche"};
        FakeTemperature wet{"humide"};
        FakePressure pressure;
        Psychrometer psychrometer;
        HumidityPsychrometer humidity;

        Bench()
        {
            psychrometer.begin(dry, wet, pressure);
            humidity.begin("hr_psy", psychrometer);
            pressure.set(SEA_LEVEL);
        }

        Psychrometer::Result measure(double_t dryValue, double_t wetValue)
        {
            dry.set(dryValue);
            wet.set(wetValue);
            humidity.update();
            return psychrometer.evaluate();
        }
    };

    TEST_CASE("psychrométrie : pression de vapeur saturante")
    {
        // Valeurs de référence IAPWS (Pa), à 0,5 % près pour Magnus.
        CHECK_NEAR(Psychrometrics::saturationVaporPressure(0.0), 611.2, 0.01);
        CHECK_NEAR(Psychrometrics::saturationVaporPressure(20.0), 2339.2, 12.0);
        CHECK_NEAR(Psychrometrics::saturationVaporPressure(-10.0), 286.5, 1.5);
        CHECK_NEAR(Psychrometrics::saturationVaporPressureIce(-10.0), 259.9, 1.3);

        // Eau et glace se rejoignent à 0 °C ; en dessous, la glace est plus basse.
        CHECK_NEAR(
            Psychrometrics::saturationVaporPressureIce(0.0),
            Psychrometrics::saturationVaporPressure(0.0),
            1e-9);
        CHECK_TRUE(
            Psychrometrics::saturationVaporPressureIce(-5.0) <
            Psychrometrics::saturationVaporPressure(-5.0));
    }

    TEST_CASE("psychrométrie : point de rosée inverse de la saturation")
    {
        for (double_t t = -20.0; t <= 80.0; t += 5.0)
            CHECK_NEAR(Psychrometrics::dewPoint(t, 100.0), t, 1e-9);

        const double_t dewPoint = Psychrometrics::dewPoint(20.0, 50.0);
        CHECK_NEAR(dewPoint, 9.255, 0.001);
        CHECK_NEAR(
            Psychrometrics::saturationVaporPressure(dewPoint),
            0.5 * Psychrometrics::saturationVaporPressure(20.0),
            1e-6);

        CHECK_FALSE(std::isfinite(Psychrometrics::dewPoint(20.0, 0.0)));
    }

    TEST_CASE("psychrométrie : humidité relative d'un psychromètre ventilé")
    {
        // Bulbes égaux : air saturé.
        CHECK_NEAR(Psychrometrics::relativeHumidity(20.0, 20.0, SEA_LEVEL), 100.0, 1e-9);

        CHECK_NEAR(Psychrometrics::relativeHumidity(20.0, 15.0, SEA_LEVEL), 58.57, 0.01);

        // En altitude, le même écart sec / humide donne plus d'humidité.
        CHECK_TRUE(
            Psychrometrics::relativeHumidity(20.0, 15.0, 85000.0) >
            Psychrometrics::relativeHumidity(20.0, 15.0, SEA_LEVEL));
    }

    TEST_CASE("psychromètre : coefficient réglable")
    {
        Bench bench;

        CHECK_NEAR(bench.measure(20.0, 15.0).relativeHumidity, 58.57, 0.01);

        // Psychromètre non ventilé.
        bench.psychrometer.settings().coefficient = 8.0;
        CHECK_NEAR(bench.measure(20.0, 15.0).relativeHumidity, 55.33, 0.01);
        CHECK_NEAR(bench.humidity.getValue(), 55.33, 0.01);
    }

    TEST_CASE("psychromètre : bulbe humide sous 0 °C")
    {
        Bench bench;

        // Par défaut, la mèche est givrée.
        CHECK_NEAR(bench.measure(-5.0, -6.0).relativeHumidity, 73.53, 0.01);

        bench.psychrometer.settings().frozenBulb =
            Psychrometer::FrozenBulb::Supercooled;
        CHECK_NEAR(bench.measure(-5.0, -6.0).relativeHumidity, 77.11, 0.01);

        // Au-dessus de 0 °C, le réglage n'a pas d'effet.
        const double_t water = bench.measure(5.0, 3.0).relativeHumidity;
        bench.psychrometer.settings().frozenBulb =
            Psychrometer::FrozenBulb::Ice;
        CHECK_NEAR(bench.measure(5.0, 3.0).relativeHumidity, water, 1e-12);
    }

    TEST_CASE("psychromètre : incohérences des bulbes")
    {
        Bench bench;

        // Bulbe humide un peu plus chaud que le sec : tolérance, 100 %.
        Psychrometer::Result result = bench.measure(20.0, 20.3);
        CHECK_TRUE(result.status == MeasurementStatus::Ok);
        CHECK_NEAR(result.relativeHumidity, 100.0, 1e-12);

        // Nettement plus chaud : sondes inversées ou décalées.
        result = bench.measure(20.0, 21.0);
        CHECK_TRUE(result.status == MeasurementStatus::Invalid);
        CHECK_FALSE(std::isfinite(result.relativeHumidity));
        CHECK_TRUE(bench.humidity.getStatus() == MeasurementStatus::Invalid);

        // Bulbe humide trop froid : humidité négative (-5,4 %).
        result = bench.measure(20.0, 5.0);
        CHECK_TRUE(result.status == MeasurementStatus::UnderRange);
        CHECK_TRUE(bench.humidity.getStatus() == MeasurementStatus::UnderRange);

        // Le défaut retombe dès que les mesures redeviennent cohérentes.
        bench.measure(20.0, 15.0);
        CHECK_TRUE(bench.humidity.isValid());
    }

    TEST_CASE("psychromètre : état des entrées")
    {
        Bench bench;

        bench.measure(20.0, 15.0);
        bench.wet.fail(MeasurementStatus::Open);
        bench.humidity.update();

        CHECK_TRUE(bench.humidity.getStatus() == MeasurementStatus::Open);
        CHECK_FALSE(std::isfinite(bench.psychrometer.dewPoint()));

        Psychrometer unwired;
        CHECK_TRUE(unwired.status() == MeasurementStatus::Invalid);
    }

    TEST_CASE("psychromètre : point de rosée et humidité absolue")
    {
        Bench bench;
        bench.measure(20.0, 15.0);

        const double_t humidity = bench.psychrometer.relativeHumidity();
        CHECK_NEAR(
            bench.psychrometer.dewPoint(),
            Psychrometrics::dewPoint(20.0, humidity),
            1e-12);

        // 20 °C, 58,6 % : environ 10,1 g/m³.
        CHECK_NEAR(bench.psychrometer.absoluteHumidity(), 10.10, 0.05);
    }

    TEST_CASE("psychromètre : paramètres du menu")
    {
        Bench bench;

        Parameter storage[4];
        ParameterList list;
        list.begin(storage, 4);

        bench.humidity.registerParameters(list);

        CHECK_FALSE(list.hasError());

        const Parameter* coefficient = list.find("hr_psy", "coefficient");
        CHECK_TRUE(coefficient != nullptr);
        CHECK_TRUE(list.find("hr_psy", "frozen_bulb") != nullptr);
    }
}
