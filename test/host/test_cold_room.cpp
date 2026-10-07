#include "TemplateBench.h"

#include <EventLog.h>
#include <Hardware/pinout.h>
#include <hardware/gpio.h>
#include <Templates/ColdRoomInstallation.h>
#include <hmi/MenuBuilder.h>

#include <cstring>

/*
 * Template chambre froide complet sur l'hôte, par cycles de 1 s :
 * compresseur et ventilateurs lus sur les relais 1 et 2, résistance sur la
 * broche PWM 1 (niveau constant à 0 / 100 %), porte sur l'entrée TOR 1.
 */
namespace
{
    // Ordre d'addSensor() dans le template.
    constexpr size_t AMBIENT = 0;
    constexpr size_t EVAPORATOR = 1;

    constexpr uint32_t INTERVAL_S = 6 * 3600;

    struct ColdRoomBench : TemplateBench<ColdRoomInstallation>
    {
        ColdRoomBench()
        {
            // Chambre à réchauffer (consigne 3 °C), évaporateur froid.
            setTemperature(AMBIENT, 6.0);
            setTemperature(EVAPORATOR, -5.0);
            setDoor(false);
        }

        void setDoor(bool open)
        {
            FakeDigitalIO::levels[Board::Rp2040::DIGITAL_INPUT_1] =
                open ? HIGH : LOW;
        }

        bool compressor() const
        {
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_1] == HIGH;
        }

        bool fans() const
        {
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_2] == HIGH;
        }

        bool heater() const
        {
            return FakeGPIO::levels[Board::Rp2040::OUTPUT_3];
        }

        bool logged(const char* text) const
        {
            const EventLog& log = process.eventLog();

            for (size_t i = 0; i < log.count(); i++)
            {
                if (std::strcmp(log.at(i)->text, text) == 0)
                    return true;
            }

            return false;
        }

        // Froid jusqu'au début du dégivrage : 6 h après le premier cycle.
        void runUntilDefrost()
        {
            const uint32_t defrostAt = 1 + INTERVAL_S;
            advance(defrostAt - now / 1000 - 10);
            CHECK_FALSE(heater());
            advance(20);
            CHECK_TRUE(heater());
        }
    };

    void testCooling()
    {
        ColdRoomBench bench;
        CHECK_TRUE(bench.started);

        // Anti-court-cycle : le compresseur attend 3 min après le démarrage.
        bench.advance(170);
        CHECK_FALSE(bench.compressor());
        bench.advance(20);
        CHECK_TRUE(bench.compressor());

        CHECK_TRUE(bench.fans());
        CHECK_FALSE(bench.heater());

        // Consigne atteinte (2 °C, bas de la bande) : arrêt.
        bench.setTemperature(AMBIENT, 1.9);
        bench.cycle();
        CHECK_FALSE(bench.compressor());
        CHECK_TRUE(bench.fans());
    }

    void testDoor()
    {
        ColdRoomBench bench;
        bench.setBool("cold_room_door_alarm", "enabled", true);
        bench.advance(10);
        CHECK_TRUE(bench.fans());

        // Porte ouverte : ventilateurs arrêtés aussitôt, alarme après 5 min.
        bench.setDoor(true);
        bench.cycle();
        CHECK_FALSE(bench.fans());

        bench.advance(295);
        CHECK_FALSE(bench.logged("Alarme Porte ouverte : ACTIVE"));
        bench.advance(10);
        CHECK_TRUE(bench.logged("Alarme Porte ouverte : ACTIVE"));

        bench.setDoor(false);
        bench.cycle();
        CHECK_TRUE(bench.fans());
    }

    void testDefrostCycle()
    {
        ColdRoomBench bench;
        bench.advance(200);
        CHECK_TRUE(bench.compressor());

        // Dégivrage après 6 h de froid : résistance, tout le reste arrêté.
        bench.runUntilDefrost();
        CHECK_FALSE(bench.compressor());
        CHECK_FALSE(bench.fans());

        // Évaporateur à 8 °C : fin du dégivrage, égouttage (tout arrêté).
        bench.setTemperature(EVAPORATOR, 9.0);
        bench.cycle();
        CHECK_FALSE(bench.heater());
        CHECK_FALSE(bench.compressor());
        CHECK_FALSE(bench.fans());

        /*
         * Égouttage de 2 min (fin vers D + 132 s, D : début du dégivrage),
         * puis reprise : compresseur seul, une fois son arrêt mini de 3 min
         * écoulé depuis D.
         */
        bench.advance(110);
        CHECK_FALSE(bench.compressor());
        bench.advance(70);
        CHECK_TRUE(bench.compressor());
        CHECK_FALSE(bench.fans());
        CHECK_FALSE(bench.heater());

        // Ventilateurs 3 min après le début de la reprise : retour au froid.
        bench.setTemperature(EVAPORATOR, -2.0);
        bench.advance(100);
        CHECK_FALSE(bench.fans());
        bench.advance(30);
        CHECK_TRUE(bench.fans());
        CHECK_TRUE(bench.compressor());

        // Prochain dégivrage 6 h plus tard.
        bench.advance(INTERVAL_S - 60);
        CHECK_FALSE(bench.heater());
    }

    void testDefrostMaximum()
    {
        // Évaporateur qui ne se réchauffe pas : fin à 30 min.
        ColdRoomBench bench;
        bench.runUntilDefrost();

        bench.advance(1780);
        CHECK_TRUE(bench.heater());
        bench.advance(20);
        CHECK_FALSE(bench.heater());
    }

    void testEvaporatorFault()
    {
        // Sonde d'évaporateur en défaut : jamais de résistance. Le dégivrage
        // se fait compresseur arrêté, et se termine par sa durée max.
        ColdRoomBench bench;
        bench.advance(200);
        bench.shortSensor(EVAPORATOR);

        bench.advance(INTERVAL_S);
        CHECK_FALSE(bench.heater());
        CHECK_FALSE(bench.compressor());
        CHECK_FALSE(bench.fans());

        // Fin par la durée max, égouttage, reprise du compresseur.
        bench.advance(1800 + 120 + 15);
        CHECK_TRUE(bench.compressor());
        CHECK_FALSE(bench.heater());
    }

    void testDefrostDisabled()
    {
        ColdRoomBench bench;
        bench.setBool("cold_room_defrost", "enabled", false);

        bench.advance(INTERVAL_S + 60);
        CHECK_FALSE(bench.heater());
        CHECK_TRUE(bench.compressor());
        CHECK_TRUE(bench.fans());
    }

    void testHighAlarmMasked()
    {
        // Chambre à 10 °C, alarme à consigne + 4 K, sans tempo.
        ColdRoomBench bench;
        bench.setBool("cold_room_high_alarm", "enabled", true);
        bench.setDiscrete("cold_room_high_alarm", "delay", 0);
        bench.setTemperature(AMBIENT, 10.0);

        // Masquée pendant 30 min de froid (démarrage, ou après un cycle).
        bench.advance(1790);
        CHECK_FALSE(bench.logged("Alarme Temp. haute : ACTIVE"));
        bench.advance(20);
        CHECK_TRUE(bench.logged("Alarme Temp. haute : ACTIVE"));
    }

    void testManualCompressorDuringDefrost()
    {
        // Le mode manuel passe avant l'inhibition du dégivrage.
        ColdRoomBench bench;
        bench.runUntilDefrost();
        CHECK_FALSE(bench.compressor());

        bench.setDiscrete(
            "cold_room_thermostat",
            "operation",
            static_cast<int32_t>(Thermostat::Operation::ForcedOn));

        // Arrêté au début du dégivrage : redémarrage après son arrêt mini
        // (3 min depuis ce début).
        bench.advance(160);
        CHECK_FALSE(bench.compressor());
        bench.advance(20);
        CHECK_TRUE(bench.compressor());
        CHECK_TRUE(bench.heater());
    }

    void testMenu()
    {
        ColdRoomBench bench;

        // Résistance : ni mode manuel, ni choix du défaut, sécurité à 0.
        CHECK_TRUE(bench.parameter("cold_room_heater", "operation") == nullptr);
        CHECK_TRUE(
            bench.parameter("cold_room_heater", "fault_action") == nullptr);

        const Parameter* safeCommand =
            bench.parameter("cold_room_heater_output", "safe_command");
        CHECK_TRUE(safeCommand != nullptr && safeCommand->readOnly);

        // Ventilateurs : mode manuel.
        CHECK_TRUE(bench.parameter("cold_room_fans", "operation") != nullptr);

        // Consigne réglable depuis l'accueil.
        CHECK_TRUE(bench.installation.homeSetpoint() != nullptr);

        // Durées et fin de dégivrage rangées sous « Dégivrage ».
        MenuBuilder menu;
        CHECK_TRUE(bench.installation.buildMenu(menu));

        const MenuBuilder::GroupId defrost =
            menu.findGroupForOwner("cold_room_defrost");
        CHECK_TRUE(defrost != MenuBuilder::INVALID_GROUP);

        const char* children[] = {
            "cold_room_defrost_end",
            "cold_room_defrost_interval",
            "cold_room_defrost_maximum",
            "cold_room_drip",
            "cold_room_fan_delay",
            "cold_room_alarm_mask"
        };

        for (const char* owner : children)
        {
            const MenuBuilder::Group* group =
                menu.getGroup(menu.findGroupForOwner(owner));
            CHECK_TRUE(group != nullptr && group->parent == defrost);
        }
    }
}

void runColdRoomTests()
{
    TestHarness::run("chambre froide : froid", testCooling);
    TestHarness::run("chambre froide : porte", testDoor);
    TestHarness::run("chambre froide : cycle de dégivrage", testDefrostCycle);
    TestHarness::run("chambre froide : durée max du dégivrage", testDefrostMaximum);
    TestHarness::run("chambre froide : sonde évaporateur en défaut", testEvaporatorFault);
    TestHarness::run("chambre froide : dégivrage désactivé", testDefrostDisabled);
    TestHarness::run("chambre froide : alarme haute masquée", testHighAlarmMasked);
    TestHarness::run("chambre froide : manuel pendant le dégivrage", testManualCompressorDuringDefrost);
    TestHarness::run("chambre froide : menu", testMenu);
}
