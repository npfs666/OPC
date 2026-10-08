// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Adafruit_BMP5xx.h>
#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <Installation.h>
#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <Regulator/Regulator.h>
#include <hmi/AlarmDisplay.h>
#include <hmi/HomeScreen.h>
#include <hmi/MenuBuilder.h>
#include <hmi/ParameterList.h>

#include <cstring>

namespace
{
    constexpr uint8_t PIN_1 = Board::Rp2040::OUTPUT_1;

    class FixedRegulator final : public Regulator
    {
    public:
        void update(uint32_t) override
        {
        }

        void set(double_t command)
        {
            writeCommand(command);
        }
    };

    void setTime(uint32_t milliseconds)
    {
        FakeTime::milliseconds = milliseconds;
    }

    void request(RelayOutput& relay, double_t command)
    {
        relay.setCommand(command, millis());
        relay.poll(millis());
    }

    void testSwitchesAndOnTime()
    {
        setTime(0);
        RelayOutput relay;
        relay.begin("relay", "Relais 1", PIN_1, true, false);
        CHECK_TRUE(relay.begin());

        const OutputCounters* counters = relay.counters();
        CHECK_TRUE(counters != nullptr);
        if (counters == nullptr)
            return;

        // L'état sûr initial (déjà OFF) n'est pas une manœuvre.
        CHECK_TRUE(counters->switches == 0);

        setTime(1000);
        request(relay, 1.0);
        CHECK_TRUE(counters->switches == 1);

        setTime(4000);
        request(relay, 0.0);
        CHECK_TRUE(counters->switches == 2);
        CHECK_NEAR(relay.onSeconds(), 3.0, 1e-9);

        // Marche en cours comptée dans la durée.
        setTime(5000);
        request(relay, 1.0);
        setTime(6500);
        CHECK_NEAR(relay.onSeconds(), 4.5, 1e-9);

        // La mise en sécurité est une vraie manœuvre.
        relay.forceSafe();
        CHECK_TRUE(counters->switches == 4);
        CHECK_NEAR(relay.onSeconds(), 4.5, 1e-9);

        // Une commande inchangée ne compte pas.
        request(relay, 0.0);
        CHECK_TRUE(counters->switches == 4);
    }

    void testResetAndRestore()
    {
        setTime(0);
        RelayOutput relay;
        relay.begin("relay", "Relais 1", PIN_1, true, false);
        relay.begin();

        relay.restoreCounters(150000, 7200.0);
        CHECK_TRUE(relay.counters()->switches == 150000);
        CHECK_NEAR(relay.onSeconds(), 7200.0, 1e-9);

        relay.restoreCounters(10, -5.0);
        CHECK_NEAR(relay.onSeconds(), 0.0, 1e-9);

        // Remise à zéro en marche : la marche en cours repart de zéro.
        setTime(1000);
        request(relay, 1.0);
        setTime(3000);
        relay.resetCounters();
        CHECK_TRUE(relay.counters()->switches == 0);
        CHECK_NEAR(relay.onSeconds(), 0.0, 1e-9);
        setTime(4000);
        CHECK_NEAR(relay.onSeconds(), 1.0, 1e-9);
    }

    void testMaintenanceLimit()
    {
        OutputCounters counters;
        counters.switches = 1000000;
        CHECK_FALSE(counters.maintenanceDue());

        counters.maintenanceLimit = 150000;
        counters.switches = 149999;
        CHECK_FALSE(counters.maintenanceDue());
        counters.switches = 150000;
        CHECK_TRUE(counters.maintenanceDue());
    }

    void testOperatingTime()
    {
        ProcessControl process;
        CHECK_NEAR(process.operatingSeconds(), 0.0, 0);

        process.restoreOperatingSeconds(3600.0);
        process.updateOperatingTime(1000);
        CHECK_NEAR(process.operatingSeconds(), 3600.0, 1e-9);

        process.updateOperatingTime(3500);
        CHECK_NEAR(process.operatingSeconds(), 3602.5, 1e-9);

        // Débordement de millis() après 49 jours.
        process.updateOperatingTime(UINT32_MAX - 499);
        process.updateOperatingTime(500);
        CHECK_NEAR(
            process.operatingSeconds(),
            3602.5 + (UINT32_MAX - 499 - 3500) / 1000.0 + 1.0,
            1e-6);

        // Valeur sauvegardée invalide : ignorée.
        process.restoreOperatingSeconds(-1.0);
        CHECK_TRUE(process.operatingSeconds() > 3600.0);
    }

    // Installation minimale : un relais sur une commande fixe.
    class CounterInstallation final : public Installation
    {
    public:
        FixedRegulator regulator;
        ActuatorOnOff actuator;
        RelayOutput relay;
        ProcessControl process;

        const char* name() const override
        {
            return "Compteurs";
        }

        const char* configurationKey() const override
        {
            return "counter_test";
        }

        bool begin(SensorBoard&, Adafruit_BMP5xx&, ProcessControl&) override
        {
            return true;
        }

        void printHomeScreen(HomeScreenContext&) override
        {
        }

        bool build()
        {
            parameterList.begin(parameterStorage, MAX_PARAMETERS);

            regulator.begin("regulator");
            actuator.begin("actuator", "Commande", regulator);
            relay.begin("relay", "Relais 1", PIN_1, true, false);

            if (!process.add(regulator) ||
                !process.add(actuator) ||
                !process.connect(actuator, relay) ||
                !process.beginOutputs())
            {
                return false;
            }

            process.registerParameters(parameterList);
            return !parameterList.hasError();
        }
    };

    void testCounterParametersAndMenu()
    {
        setTime(0);
        CounterInstallation installation;
        CHECK_TRUE(installation.build());

        const ParameterList& parameters = installation.getParameters();

        const Parameter* switches =
            parameters.find("relay.counters", "switches");
        const Parameter* onHours =
            parameters.find("relay.counters", "on_hours");
        const Parameter* limit =
            parameters.find("relay.counters", "maintenance_limit");

        CHECK_TRUE(switches != nullptr && switches->readOnly && !switches->persistent);
        CHECK_TRUE(onHours != nullptr && onHours->readOnly && !onHours->persistent);
        CHECK_TRUE(limit != nullptr && !limit->readOnly && limit->persistent);
        CHECK_TRUE(switches != nullptr &&
                   std::strcmp(switches->parentOwnerKey, "counters") == 0);

        // Divers > Compteurs > Relais 1, avec son action de remise à zéro.
        MenuBuilder menu;
        CHECK_TRUE(installation.buildMenu(menu));
        CHECK_TRUE(installation.process.addMenuActions(menu));

        const MenuBuilder::GroupId counters = menu.findGroupForOwner("counters");
        const MenuBuilder::GroupId relayCounters =
            menu.findGroupForOwner("relay.counters");
        CHECK_TRUE(counters != MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(relayCounters != MenuBuilder::INVALID_GROUP);

        const MenuBuilder::Group* group = menu.getGroup(relayCounters);
        CHECK_TRUE(group != nullptr && group->parent == counters);

        const MenuBuilder::Action* reset = menu.findAction("reset_counters_1");
        CHECK_TRUE(reset != nullptr && reset->group == relayCounters);
    }

    void testResetActionEventsAndBanner()
    {
        setTime(0);
        CounterInstallation installation;
        CHECK_TRUE(installation.build());

        ProcessControl& process = installation.process;
        RelayOutput& relay = installation.relay;

        const MenuBuilder::ActionId reset =
            ProcessControl::RESET_COUNTERS_ACTION;

        CHECK_TRUE(process.handlesMenuAction(reset));
        CHECK_FALSE(process.handlesMenuAction(reset + 1));
        CHECK_FALSE(process.takeCountersChanged());

        Stream log;
        ProcessSnapshot snapshot;
        char text[32];
        uint16_t color = 0;

        // Seuil à 4 manœuvres (deux marches, deux arrêts). Il se règle dans
        // le menu, par le paramètre lié au compteur.
        const_cast<OutputCounters*>(relay.counters())->maintenanceLimit = 4;

        for (uint32_t i = 0; i < 4; i++)
        {
            setTime(1000 * (i + 1));
            installation.regulator.set(i % 2 == 0 ? 1.0 : 0.0);
            process.updateMeasurementsAndRegulators(millis());
        }

        process.updateMeasurementsAndRegulators(10000);
        CHECK_TRUE(relay.counters()->switches == 4);

        process.captureSnapshot(snapshot, 10000);
        const OutputSample* sample = snapshot.find(relay);
        CHECK_TRUE(sample != nullptr && sample->maintenanceDue);

        AlarmDisplay::format(snapshot, text, sizeof(text), 18, color);
        CHECK_TRUE(std::strcmp(text, "ENTRETIEN Relais 1") == 0);
        CHECK_TRUE(color == AlarmDisplay::COLOR_LATCHED);

        // Journalisé une seule fois.
        process.updateMeasurementsAndRegulators(11000);
        process.printStatusEvents(log);
        CHECK_TRUE(log.printedLineCount == 1);

        // Remise à zéro par l'action du menu : plus d'entretien, à sauvegarder.
        CHECK_TRUE(process.executeMenuAction(reset));
        CHECK_TRUE(relay.counters()->switches == 0);
        CHECK_TRUE(process.takeCountersChanged());
        CHECK_FALSE(process.takeCountersChanged());

        process.updateMeasurementsAndRegulators(12000);
        process.captureSnapshot(snapshot, 12000);
        CHECK_TRUE(AlarmDisplay::format(snapshot, text, sizeof(text), 18, color) == 0);
    }
}

void runCounterTests()
{
    TestHarness::run("compteurs : manoeuvres et heures en marche", testSwitchesAndOnTime);
    TestHarness::run("compteurs : remise a zero et relecture", testResetAndRestore);
    TestHarness::run("compteurs : seuil d'entretien", testMaintenanceLimit);
    TestHarness::run("compteurs : heures de fonctionnement", testOperatingTime);
    TestHarness::run("compteurs : parametres et menu", testCounterParametersAndMenu);
    TestHarness::run("compteurs : RAZ, journal et bandeau", testResetActionEventsAndBanner);
}
