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

    TEST_CASE("Four à l'arrêt et menu")
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

        // Sécurités actives par défaut : boucle ouverte et délais maxi.
        const Parameter* loop = bench.parameter("kiln_loop", "enabled");
        CHECK_TRUE(loop != nullptr && *loop->value.boolean);

        const Parameter* maxHold = bench.parameter("kiln_program", "max_hold");
        CHECK_TRUE(maxHold != nullptr &&
                   maxHold->discrete.read(maxHold->discrete.target) == 120);

        const Parameter* maxDuration =
            bench.parameter("kiln_program", "max_duration");
        CHECK_TRUE(maxDuration != nullptr &&
                   maxDuration->discrete.read(maxDuration->discrete.target) == 20);

        MenuBuilder menu;
        CHECK_TRUE(bench.installation.buildMenu(menu));
        CHECK_TRUE(menu.findAction(KilnInstallation::START_ACTION) != nullptr);
        CHECK_TRUE(menu.findAction(KilnInstallation::SKIP_ACTION) != nullptr);
    }

    TEST_CASE("Four cuisson complète")
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

    TEST_CASE("Four surchauffe")
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

    TEST_CASE("Four écart haut")
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

    TEST_CASE("Four refroidissement libre")
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

    TEST_CASE("Four thermocouple hors du four")
    {
        // Le thermocouple est tombé : il lit l'ambiance pendant que le PID
        // chauffe à pleine puissance.
        KilnBench bench;
        bench.oneSegment(0.0, 1000.0, 60);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(19 * 60);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_FALSE(bench.logged("boucle ouverte"));

        // Moins de 2 °C de progrès en 2 × Ti (20 min) : boucle ouverte, la
        // cuisson s'arrête et le contacteur s'ouvre.
        bench.advance(2 * 60);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) == 0);
        CHECK_TRUE(bench.logged("Boucle four : ACTIVE"));
        CHECK_TRUE(bench.logged("Cuisson arrêtée : boucle ouverte"));

        // Alarme mémorisée : pas de nouveau départ avant l'acquittement.
        CHECK_FALSE(bench.action(KilnInstallation::START_ACTION));

        bench.process.acknowledgeAlarms();
        bench.advance(1);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(5);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) > 0);
    }

    TEST_CASE("Four relais statique collé en descente")
    {
        // Descente contrôlée de 900 à 500 °C à 100 °C/h : le four ne
        // refroidit pas, le relais statique est collé.
        KilnBench bench;
        bench.oneSegment(100.0, 500.0, 0);
        bench.setKiln(900.0);
        bench.advance(2);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(15 * 60);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) == 0);

        // Commande à 0 % sans descente pendant 20 min : boucle ouverte.
        bench.advance(10 * 60);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.logged("Cuisson arrêtée : boucle ouverte"));
        CHECK_FALSE(bench.logged("Écart haut"));
    }

    TEST_CASE("Four attente max")
    {
        // Écart maxi de 20 °C : le four froid bloque la rampe dès 2 min.
        KilnBench bench;
        bench.oneSegment(600.0, 600.0, 0);
        bench.setNumber("kiln_program", "holdback", 20.0);
        bench.setDiscrete("kiln_program", "max_hold", 5);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(6 * 60);
        CHECK_TRUE(bench.contactorClosed());

        // Bloqué plus de 5 min : la cuisson s'arrête.
        bench.advance(2 * 60);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.logged("Cuisson arrêtée : attente max"));
        CHECK_TRUE(bench.logged("Durée dépassée : ACTIVE"));
        CHECK_FALSE(bench.logged("boucle ouverte"));
    }

    TEST_CASE("Four durée max")
    {
        // Palier de 2 h à 30 °C, durée maxi de 1 h.
        KilnBench bench;
        bench.oneSegment(0.0, 30.0, 120);
        bench.setDiscrete("kiln_program", "max_duration", 1);
        bench.setKiln(30.0);
        bench.advance(2);
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(59 * 60);
        CHECK_TRUE(bench.contactorClosed());

        bench.advance(2 * 60);
        CHECK_FALSE(bench.contactorClosed());
        CHECK_TRUE(bench.logged("Cuisson arrêtée : durée max"));
        CHECK_TRUE(bench.logged("Durée dépassée : ACTIVE"));

        // Démarrer relance la cuisson et efface l'alarme.
        CHECK_TRUE(bench.action(KilnInstallation::START_ACTION));
        bench.advance(5);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_TRUE(bench.logged("Durée dépassée : FIN"));
    }

    TEST_CASE("Four défaut thermocouple")
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
