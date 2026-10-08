// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TemplateBench.h"

#include <Hardware/pinout.h>
#include <Physics/Thermocouple.h>
#include <Templates/KilnInstallation.h>
#include <hardware/gpio.h>

#include <cmath>
#include <cstring>

/*
 * Template four céramique complet sur l'hôte : thermocouple K simulé par sa
 * tension (soudure froide à 25 °C), relais statique sur la sortie DC 1,
 * contacteur sur le relais 1.
 */
namespace
{
    namespace Tc = Physics::Thermocouple;

    constexpr double_t COLD_JUNCTION = 25.0;

    struct KilnBench : TemplateBench<KilnInstallation>
    {
        // Premières mesures faites : un départ est possible.
        KilnBench()
        {
            setKiln(COLD_JUNCTION);
            advance(2);
        }

        void setKiln(double_t celsius)
        {
            board.adcTemperature = COLD_JUNCTION;
            board.voltageMv =
                Tc::temperatureToMillivolts(Tc::Type::K, celsius) -
                Tc::temperatureToMillivolts(Tc::Type::K, COLD_JUNCTION);
        }

        // Thermocouple coupé : plus de tension lisible.
        void openThermocouple()
        {
            board.voltageMv = NAN;
        }

        bool contactorClosed() const
        {
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_1] == HIGH;
        }

        bool ssrOn() const
        {
            return FakeGPIO::levels[Board::Rp2040::OUTPUT_3];
        }

        // Secondes de chauffe du relais statique pendant la durée donnée.
        uint32_t heatingDuring(uint32_t seconds)
        {
            uint32_t on = 0;

            for (uint32_t i = 0; i < seconds; i++)
            {
                cycle();

                if (ssrOn())
                    on++;
            }

            return on;
        }

        bool action(MenuBuilder::ActionId id)
        {
            return installation.executeMenuAction(id);
        }

        bool logged(const char* text)
        {
            const EventLog& log = process.eventLog();

            for (size_t i = 0; i < log.count(); i++)
            {
                const EventEntry* entry = log.at(i);

                if (entry != nullptr && std::strstr(entry->text, text) != nullptr)
                    return true;
            }

            return false;
        }

        // Programme 1 réduit à un segment.
        void oneSegment(double_t rate, double_t target, uint32_t soak)
        {
            setDiscrete("kiln_program.p1", "segments", 1);
            setNumber("kiln_program.p1.s1", "rate", rate);
            setNumber("kiln_program.p1.s1", "target", target);
            setDiscrete("kiln_program.p1.s1", "soak", static_cast<int32_t>(soak));
        }
    };

    void testIdle()
    {
        KilnBench bench;
        CHECK_TRUE(bench.started);

        // À l'arrêt : contacteur ouvert, aucune chauffe.
        CHECK_TRUE(bench.heatingDuring(30) == 0);
        CHECK_FALSE(bench.contactorClosed());

        // Trois programmes, segments et actions au menu.
        CHECK_TRUE(bench.parameter("kiln_program.p3.s8", "target") != nullptr);
        CHECK_TRUE(bench.parameter("kiln_program.p4", "segments") == nullptr);
        CHECK_TRUE(bench.parameter("kiln_pid", "setpoint") == nullptr);
        CHECK_TRUE(bench.parameter("kiln_pid", "fault_action") == nullptr);
        CHECK_TRUE(bench.parameter("kiln_contactor_cmd", "operation") == nullptr);

        // Place pour les réglages de la carte, absents du banc : sonde,
        // calibrations et horloge (une trentaine).
        CHECK_TRUE(
            bench.installation.getParameters().count() <= MAX_PARAMETERS - 40);

        // Seuil de surchauffe réglable au-delà de 1000 °C.
        const Parameter* limit = bench.parameter("kiln_overtemp", "limit");
        CHECK_TRUE(limit != nullptr && *limit->value.number == 1300.0);

        MenuBuilder menu;
        CHECK_TRUE(bench.installation.buildMenu(menu));
        CHECK_TRUE(menu.findAction(KilnInstallation::START_ACTION) != nullptr);
        CHECK_TRUE(menu.findAction(KilnInstallation::SKIP_ACTION) != nullptr);
    }

    void testFiring()
    {
        KilnBench bench;
        bench.oneSegment(600.0, 60.0, 2);

        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));

        // 600 °C/h : 10 °C par minute. Le four reste froid : le PID chauffe,
        // le contacteur est fermé.
        CHECK_TRUE(bench.heatingDuring(120) > 0);
        CHECK_TRUE(bench.contactorClosed());

        // Four arrivé : palier de 2 min, puis fin de cuisson.
        bench.setKiln(60.0);
        bench.advance(180);
        CHECK_TRUE(bench.contactorClosed());
        bench.advance(120);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) == 0);
        CHECK_TRUE(bench.logged("terminée"));
    }

    void testOverTemperature()
    {
        KilnBench bench;
        bench.oneSegment(0.0, 1250.0, 60);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.setKiln(1200.0);
        bench.advance(30);
        CHECK_TRUE(bench.contactorClosed());

        // Surchauffe : la cuisson s'arrête, le contacteur s'ouvre.
        bench.setKiln(1310.0);
        bench.advance(15);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.logged("surchauffe"));

        // Alarme mémorisée : pas de nouveau départ avant l'acquittement.
        bench.setKiln(800.0);
        bench.advance(5);
        CHECK_FALSE(bench.action(KilnInstallation::START_ACTION));

        bench.process.acknowledgeAlarms();
        bench.advance(1);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(5);
        CHECK_TRUE(bench.contactorClosed());
    }

    void testDeviation()
    {
        KilnBench bench;
        bench.oneSegment(60.0, 600.0, 0);
        bench.setKiln(400.0);
        bench.advance(2);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(60);
        CHECK_TRUE(bench.contactorClosed());

        // Relais statique collé : le four s'emballe, le contacteur s'ouvre
        // après 2 min et reste ouvert.
        bench.setKiln(500.0);
        bench.advance(110);
        CHECK_TRUE(bench.contactorClosed());
        bench.advance(20);
        CHECK_FALSE(bench.contactorClosed());

        bench.setKiln(400.0);
        bench.advance(30);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_FALSE(bench.action(KilnInstallation::START_ACTION));

        bench.process.acknowledgeAlarms();
        bench.advance(2);
        CHECK_TRUE(bench.contactorClosed());
    }

    void testCoolingSegment()
    {
        // Refroidissement libre de 900 à 600 °C : le four au-dessus de la
        // consigne n'est pas une alarme.
        KilnBench bench;
        bench.oneSegment(0.0, 600.0, 10);
        bench.setKiln(900.0);
        bench.advance(2);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        CHECK_TRUE(bench.heatingDuring(600) == 0);
        CHECK_TRUE(bench.contactorClosed());

        bench.setKiln(600.0);
        bench.advance(30);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_FALSE(bench.logged("Écart haut"));
    }

    void testThermocoupleFault()
    {
        KilnBench bench;
        bench.oneSegment(0.0, 1000.0, 60);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.setKiln(500.0);
        bench.advance(30);
        CHECK_TRUE(bench.contactorClosed());

        // Thermocouple coupé : tout s'arrête, sans maintien.
        bench.openThermocouple();
        bench.advance(2);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) == 0);

        // Retour du thermocouple : la cuisson reprend.
        bench.setKiln(500.0);
        bench.advance(5);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) > 0);

        // Thermocouple invalide : pas de départ.
        CHECK_TRUE(bench.action(KilnInstallation::STOP_ACTION));
        bench.openThermocouple();
        bench.advance(2);
        CHECK_FALSE(bench.action(KilnInstallation::START_ACTION));
    }
}

void runKilnTests()
{
    TestHarness::run("Four à l'arrêt et menu", testIdle);
    TestHarness::run("Four cuisson complète", testFiring);
    TestHarness::run("Four surchauffe", testOverTemperature);
    TestHarness::run("Four écart haut", testDeviation);
    TestHarness::run("Four refroidissement libre", testCoolingSegment);
    TestHarness::run("Four défaut thermocouple", testThermocoupleFault);
}
