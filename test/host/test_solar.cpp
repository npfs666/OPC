#include "TestHarness.h"
#include "InstallationTestAccess.h"

#include <Adafruit_BMP5xx.h>
#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <Physics/PT100.h>
#include <ProcessControl.h>
#include <Templates/SolarInstallation.h>
#include <hmi/MenuBuilder.h>

#include <cstring>

/*
 * Template solaire complet sur l'hôte : sondes simulées par leur résistance,
 * pompe lue sur la broche du relais 1. Les valeurs évitent les égalités
 * exactes aux seuils, que la conversion résistance -> température ne
 * conserve pas ; les seuils compris sont testés dans test_comparator.cpp.
 */
namespace
{
    constexpr uint8_t MONDAY = 1;

    // Ordre d'addSensor() dans le template.
    constexpr size_t COLLECTOR = 0;
    constexpr size_t TANK_TOP = 1;
    constexpr size_t TANK_BOTTOM = 2;

    struct SolarBench
    {
        SensorBoard board;
        Adafruit_BMP5xx bmp580;
        ProcessControl process;
        SolarInstallation solar;
        ClockSample clock;
        uint32_t now = 0;
        bool started = false;

        SolarBench()
        {
            started = InstallationTestAccess::start(
                solar, board, bmp580, process);
            setClock(MONDAY, 12);
        }

        void setClock(uint8_t dayOfWeek, uint8_t hour, bool valid = true)
        {
            clock.dateTime.dayOfWeek = dayOfWeek;
            clock.dateTime.hour = hour;
            clock.dateTime.minute = 0;
            clock.dateTime.second = 0;
            clock.valid = valid;
        }

        void setTemperature(size_t sensor, double_t celsius)
        {
            board.sensorResistances[sensor] =
                PT100::getTemperatureToResistance(celsius);
        }

        void setTemperatures(
            double_t collector,
            double_t tankTop,
            double_t tankBottom)
        {
            setTemperature(COLLECTOR, collector);
            setTemperature(TANK_TOP, tankTop);
            setTemperature(TANK_BOTTOM, tankBottom);
        }

        // Sonde en court-circuit.
        void shortSensor(size_t sensor)
        {
            board.sensorResistances[sensor] = 5.0;
        }

        // Un cycle de mesure, puis l'état de la pompe (relais 1).
        bool pump()
        {
            now += 1000;
            process.updateClock(clock);
            process.updateMeasurementsAndRegulators(now);
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_1] == HIGH;
        }

        const Parameter* parameter(const char* owner, const char* key)
        {
            return solar.getParameters().find(owner, key);
        }

        void setNumber(const char* owner, const char* key, double_t value)
        {
            const Parameter* found = parameter(owner, key);
            CHECK_TRUE(found != nullptr);

            if (found != nullptr)
                *found->value.number = value;
        }

        void setBool(const char* owner, const char* key, bool value)
        {
            const Parameter* found = parameter(owner, key);
            CHECK_TRUE(found != nullptr);

            if (found != nullptr)
                *found->value.boolean = value;
        }

        void setSelection(const char* owner, const char* key, int32_t value)
        {
            const Parameter* found = parameter(owner, key);
            CHECK_TRUE(found != nullptr);

            if (found != nullptr)
                found->discrete.write(found->discrete.target, value);
        }
    };

    void testCharge()
    {
        SolarBench bench;
        CHECK_TRUE(bench.started);

        // Capteur 10 K plus chaud que le bas du ballon : charge.
        bench.setTemperatures(50.0, 40.0, 40.0);
        CHECK_TRUE(bench.pump());

        // Écart de 5 K, entre arrêt (4 K) et démarrage (8 K) : continue.
        bench.setTemperature(COLLECTOR, 45.0);
        CHECK_TRUE(bench.pump());

        // Sous le delta d'arrêt.
        bench.setTemperature(COLLECTOR, 43.9);
        CHECK_FALSE(bench.pump());

        // Ballon plein (haut au-delà de 80 °C) : pas de charge.
        bench.setTemperatures(60.0, 80.5, 40.0);
        CHECK_FALSE(bench.pump());

        // Capteur sous 20 °C : pas de charge, même avec un écart suffisant.
        bench.setTemperatures(19.5, 5.0, 5.0);
        CHECK_FALSE(bench.pump());

        // Sonde en défaut : la pompe s'arrête.
        bench.setTemperatures(60.0, 40.0, 40.0);
        CHECK_TRUE(bench.pump());
        bench.shortSensor(COLLECTOR);
        CHECK_FALSE(bench.pump());
    }

    void testChargeAfterLimit()
    {
        SolarBench bench;

        bench.setTemperatures(50.0, 40.0, 40.0);
        CHECK_TRUE(bench.pump());

        bench.setTemperature(TANK_TOP, 80.5);
        CHECK_FALSE(bench.pump());

        /*
         * Limite levée avec un écart de 5 K, entre arrêt et démarrage : le
         * différentiel est resté en marche, la charge reprend. L'ancien
         * SolarRegulator attendait de nouveau l'écart de démarrage.
         */
        bench.setTemperatures(45.0, 79.5, 40.0);
        CHECK_TRUE(bench.pump());
    }

    void testHolidayDischarge()
    {
        SolarBench bench;

        // Nuit, ballon chaud, capteur froid, mode vacances désactivé.
        bench.setClock(MONDAY, 2);
        bench.setTemperatures(20.0, 75.0, 70.0);
        CHECK_FALSE(bench.pump());

        // Mode vacances : décharge jusqu'à 50 °C en bas de ballon.
        bench.setBool("solar_holiday", "holiday_mode", true);
        CHECK_TRUE(bench.pump());

        bench.setTemperature(TANK_BOTTOM, 50.5);
        CHECK_TRUE(bench.pump());

        bench.setTemperature(TANK_BOTTOM, 49.9);
        CHECK_FALSE(bench.pump());

        // Hystérésis : reprise seulement dès 52 °C.
        bench.setTemperature(TANK_BOTTOM, 51.5);
        CHECK_FALSE(bench.pump());

        bench.setTemperature(TANK_BOTTOM, 52.1);
        CHECK_TRUE(bench.pump());

        // Capteur qui se réchauffe : arrêt sous le delta d'arrêt (4 K).
        bench.setTemperature(COLLECTOR, 48.5);
        CHECK_FALSE(bench.pump());

        // Le jour, hors plage : pas de décharge.
        bench.setTemperatures(20.0, 75.0, 70.0);
        bench.setClock(MONDAY, 12);
        CHECK_FALSE(bench.pump());

        // Heure inconnue : pas de décharge...
        bench.setClock(MONDAY, 2, false);
        CHECK_FALSE(bench.pump());

        // ... mais la charge solaire continue.
        bench.setTemperatures(60.0, 45.0, 40.0);
        CHECK_TRUE(bench.pump());

        // Sonde en défaut : tout s'arrête.
        bench.shortSensor(TANK_BOTTOM);
        CHECK_FALSE(bench.pump());
    }

    void testDischargeSettings()
    {
        SolarBench bench;
        bench.setClock(MONDAY, 2);
        bench.setBool("solar_holiday", "holiday_mode", true);

        // Décharge réglée jusqu'à 60 °C (dès 62 °C) : à 61 °C, rien.
        bench.setNumber("solar_discharge_tank", "on_threshold", 62.0);
        bench.setNumber("solar_discharge_tank", "off_threshold", 60.0);
        bench.setTemperatures(20.0, 75.0, 61.0);
        CHECK_FALSE(bench.pump());

        bench.setTemperature(TANK_BOTTOM, 62.5);
        CHECK_TRUE(bench.pump());
    }

    void testManualPump()
    {
        SolarBench bench;
        bench.setTemperatures(20.0, 40.0, 40.0);
        CHECK_FALSE(bench.pump());

        // Commande « Marche » de la pompe : forcée, sans condition.
        bench.setSelection(
            "solar_pump_command",
            "operation",
            static_cast<int32_t>(LogicCommand::Operation::ForcedOn));
        CHECK_TRUE(bench.pump());

        bench.setSelection(
            "solar_pump_command",
            "operation",
            static_cast<int32_t>(LogicCommand::Operation::Auto));
        CHECK_FALSE(bench.pump());
    }

    void testMenu()
    {
        SolarBench bench;

        // Réglages de la pompe : commande seulement (défaut verrouillé).
        CHECK_TRUE(bench.parameter("solar_pump_command", "operation") != nullptr);
        CHECK_TRUE(
            bench.parameter("solar_pump_command", "fault_action") == nullptr);

        // Seuil unique pour les limites.
        CHECK_TRUE(bench.parameter("solar_tank_max", "on_threshold") != nullptr);
        CHECK_TRUE(bench.parameter("solar_tank_max", "off_threshold") == nullptr);

        // Conditions rangées sous « Pompe solaire » et « Vacances ».
        MenuBuilder menu;
        CHECK_TRUE(bench.solar.buildMenu(menu));

        const MenuBuilder::GroupId pumpGroup =
            menu.findGroupForOwner("solar_pump_command");
        const MenuBuilder::GroupId holidayGroup =
            menu.findGroupForOwner("solar_holiday");

        CHECK_TRUE(pumpGroup != MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(holidayGroup != MenuBuilder::INVALID_GROUP);

        const char* pumpChildren[] = {
            "solar_charge", "solar_tank_max", "solar_collector_min"
        };

        for (const char* owner : pumpChildren)
        {
            const MenuBuilder::Group* group =
                menu.getGroup(menu.findGroupForOwner(owner));
            CHECK_TRUE(group != nullptr && group->parent == pumpGroup);
        }

        const char* holidayChildren[] = {
            "solar_discharge_tank", "solar_discharge_delta"
        };

        for (const char* owner : holidayChildren)
        {
            const MenuBuilder::Group* group =
                menu.getGroup(menu.findGroupForOwner(owner));
            CHECK_TRUE(group != nullptr && group->parent == holidayGroup);
        }
    }
}

void runSolarTests()
{
    TestHarness::run("solaire : charge", testCharge);
    TestHarness::run("solaire : charge après une limite", testChargeAfterLimit);
    TestHarness::run("solaire : décharge nocturne vacances", testHolidayDischarge);
    TestHarness::run("solaire : réglages de la décharge", testDischargeSettings);
    TestHarness::run("solaire : pompe en manuel", testManualPump);
    TestHarness::run("solaire : menu", testMenu);
}
