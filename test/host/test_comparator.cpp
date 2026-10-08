// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Measurements/Temperature/Temperature.h>
#include <Regulator/Comparator.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    using Status = MeasurementStatus;
    using Direction = Comparator::Direction;

    class ControlledTemperature final : public Temperature
    {
    public:
        ControlledTemperature()
        {
            begin("temperature");
        }

        void update() override
        {
        }

        void set(double_t value)
        {
            setValue(value);
            setStatus(Status::Ok);
        }

        void fail(Status status)
        {
            setValue(NAN);
            setStatus(status);
        }
    };

    // Applique une valeur et met à jour le comparateur.
    bool step(
        Comparator& comparator,
        ControlledTemperature& temperature,
        double_t value,
        uint32_t now)
    {
        temperature.set(value);
        comparator.update(now);
        return comparator.isOn();
    }

    void testAbove()
    {
        ControlledTemperature temperature;
        Comparator comparator;
        comparator.begin(
            "seuil", "Seuil", temperature, Direction::Above, 50.0, 45.0);

        CHECK_FALSE(step(comparator, temperature, 40.0, 0));
        CHECK_TRUE(comparator.isCommandValid());
        CHECK_NEAR(comparator.readCommand(), 0.0, 0.0);

        // Marche au seuil de marche, seuil compris.
        CHECK_TRUE(step(comparator, temperature, 50.0, 1000));
        CHECK_NEAR(comparator.readCommand(), 1.0, 0.0);

        // Dans la bande : état conservé.
        CHECK_TRUE(step(comparator, temperature, 47.0, 2000));

        // Arrêt au seuil d'arrêt, seuil compris.
        CHECK_FALSE(step(comparator, temperature, 45.0, 3000));
        CHECK_FALSE(step(comparator, temperature, 47.0, 4000));

        double_t value = 0.0;
        CHECK_TRUE(comparator.readValue(value));
        CHECK_NEAR(value, 47.0, 0.0);
    }

    void testBelow()
    {
        // Hors-gel : marche à 5 °C, arrêt à 7 °C.
        ControlledTemperature temperature;
        Comparator comparator;
        comparator.begin(
            "hors_gel", "Hors-gel", temperature, Direction::Below, 5.0, 7.0);

        CHECK_FALSE(step(comparator, temperature, 6.0, 0));
        CHECK_TRUE(step(comparator, temperature, 5.0, 1000));
        CHECK_TRUE(step(comparator, temperature, 6.5, 2000));
        CHECK_FALSE(step(comparator, temperature, 7.0, 3000));
        CHECK_FALSE(step(comparator, temperature, 6.0, 4000));
    }

    void testSingleThreshold()
    {
        ControlledTemperature temperature;
        Comparator comparator;
        comparator.begin(
            "max", "Max", temperature, Direction::Above, 80.0, 70.0);
        comparator.useSingleThreshold();

        // Sans hystérésis : le seuil d'arrêt suit celui de marche.
        CHECK_FALSE(step(comparator, temperature, 79.9, 0));
        CHECK_TRUE(step(comparator, temperature, 80.0, 1000));
        CHECK_FALSE(step(comparator, temperature, 79.9, 2000));

        comparator.settings.onThreshold = 60.0;
        CHECK_TRUE(step(comparator, temperature, 60.0, 3000));
        CHECK_NEAR(comparator.settings.offThreshold, 60.0, 0.0);
        CHECK_FALSE(step(comparator, temperature, 59.9, 4000));

        // Un seul réglage dans le menu.
        Parameter storage[4];
        ParameterList list;
        list.begin(storage, 4);
        comparator.registerParameters(list);
        CHECK_FALSE(list.hasError());
        CHECK_TRUE(list.find("max", "on_threshold") != nullptr);
        CHECK_TRUE(list.find("max", "off_threshold") == nullptr);
    }

    void testDifferential()
    {
        // Charge solaire : capteur - bas du ballon, marche 8 K, arrêt 4 K.
        ControlledTemperature collector;
        ControlledTemperature tank;
        Comparator comparator;
        comparator.begin(
            "charge", "Charge", collector, tank, Direction::Above, 8.0, 4.0);

        tank.set(40.0);
        CHECK_TRUE(step(comparator, collector, 50.0, 0));
        CHECK_TRUE(step(comparator, collector, 45.0, 1000));
        CHECK_FALSE(step(comparator, collector, 44.0, 2000));

        double_t value = 0.0;
        CHECK_TRUE(comparator.readValue(value));
        CHECK_NEAR(value, 4.0, 1e-12);

        // Défaut de la seconde mesure.
        tank.fail(Status::Short);
        CHECK_FALSE(step(comparator, collector, 60.0, 3000));
        CHECK_FALSE(comparator.isCommandValid());

        // Un écart de °C se règle en kelvins.
        Parameter storage[4];
        ParameterList list;
        list.begin(storage, 4);
        comparator.registerParameters(list);
        const Parameter* onThreshold = list.find("charge", "on_threshold");
        CHECK_TRUE(onThreshold != nullptr);
        CHECK_TRUE(
            onThreshold != nullptr &&
            std::strcmp(onThreshold->data.number.unit, "K") == 0);
    }

    void testFaultAndResume()
    {
        ControlledTemperature temperature;
        Comparator comparator;
        comparator.begin(
            "seuil", "Seuil", temperature, Direction::Above, 50.0, 45.0);

        CHECK_TRUE(step(comparator, temperature, 55.0, 0));

        // Mesure en défaut : commande invalide.
        temperature.fail(Status::Open);
        comparator.update(1000);
        CHECK_FALSE(comparator.isCommandValid());
        CHECK_FALSE(comparator.isOn());

        double_t value = 0.0;
        CHECK_FALSE(comparator.readValue(value));

        // Retour dans la bande : repart de l'arrêt.
        CHECK_FALSE(step(comparator, temperature, 47.0, 2000));
        CHECK_TRUE(comparator.isCommandValid());

        // Même chose après une reprise (menu).
        CHECK_TRUE(step(comparator, temperature, 55.0, 3000));
        comparator.resume(4000);
        CHECK_FALSE(comparator.isCommandValid());
        CHECK_FALSE(step(comparator, temperature, 47.0, 5000));
    }

    void testParameters()
    {
        ControlledTemperature temperature;
        Comparator comparator;
        comparator.begin(
            "seuil", "Seuil", temperature, Direction::Above, 50.0, 45.0);
        comparator.setRange(0.0, 100.0, 1.0, 0);
        comparator.setLabels("Delta démarrage", "Delta arrêt");

        Parameter storage[4];
        ParameterList list;
        list.begin(storage, 4);
        comparator.registerParameters(list);
        CHECK_FALSE(list.hasError());

        const Parameter* onThreshold = list.find("seuil", "on_threshold");
        CHECK_TRUE(onThreshold != nullptr);
        CHECK_TRUE(
            onThreshold != nullptr &&
            std::strcmp(onThreshold->name, "Delta démarrage") == 0);
        CHECK_TRUE(
            onThreshold != nullptr &&
            onThreshold->data.number.maximum == 100.0);
        CHECK_TRUE(list.find("seuil", "off_threshold") != nullptr);

        // Le seuil d'arrêt doit être du côté où le comparateur retombe.
        ParameterEditor editor;
        editor.begin(list);
        editor.capture();
        CHECK_TRUE(comparator.validateParameters(editor));

        comparator.settings.offThreshold = 55.0;
        editor.capture();
        CHECK_FALSE(comparator.validateParameters(editor));

        // Seuils égaux : comparaison simple, accepté.
        comparator.settings.offThreshold = 50.0;
        editor.capture();
        CHECK_TRUE(comparator.validateParameters(editor));
    }
}

void runComparatorTests()
{
    TestHarness::run("comparateur : au-dessus", testAbove);
    TestHarness::run("comparateur : en dessous", testBelow);
    TestHarness::run("comparateur : seuil unique", testSingleThreshold);
    TestHarness::run("comparateur : différentiel", testDifferential);
    TestHarness::run("comparateur : défaut et reprise", testFaultAndResume);
    TestHarness::run("comparateur : paramètres", testParameters);
}
