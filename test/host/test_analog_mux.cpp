// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Hardware/AnalogMux.h>
#include <Hardware/AnalogRoute.h>
#include <Hardware/pinout.h>
#include <Wire.h>

#include <vector>

/*
 * Routage analogique de la carte v0.2 : octets des ports du MCP23017 et
 * séquence d'écriture du pilote. Les octets attendus reprennent le routage
 * de l'ancien pilote broche par broche (doc/Plan_acquisition_hors_ISR.md,
 * § 4).
 */
namespace
{
    using Type = Sensor::Type;
    using Wiring = Sensor::Wiring;
    using Bytes = std::vector<uint8_t>;

    constexpr uint8_t OLATA = 0x14;
    constexpr uint8_t OLATB = 0x15;
    constexpr uint8_t IODIRA = 0x00;
    constexpr uint8_t IODIRB = 0x01;
    constexpr uint8_t IOCON = 0x0A;

    Sensor::Settings settingsFor(Type type, Wiring wiring)
    {
        return Sensor::Settings{type, wiring, 0.0, 16};
    }

    struct ExpectedRoute
    {
        Type type;
        Wiring wiring;
        uint8_t portA;
    };

    constexpr ExpectedRoute ROUTES[] = {
        {Type::Pt100, Wiring::TwoWire, 0x15},
        {Type::Pt1000, Wiring::TwoWire, 0x91},
        {Type::Tc, Wiring::TwoWire, 0x59},
        {Type::Pt100, Wiring::ThreeWire, 0x25},
        {Type::Pt1000, Wiring::ThreeWire, 0xA1},
        {Type::Pt100, Wiring::FourWire, 0x06},
        {Type::Pt1000, Wiring::FourWire, 0x82},
    };

    // Bus simulé sur lequel répond le MCP23017 de la carte.
    TwoWire mcpBus()
    {
        TwoWire wire;
        wire.deviceAddress = Board::Mcp23017::ADDRESS;
        wire.mcpPinsFollowLatches = true;
        return wire;
    }

    // Reset du MCP (ESD, chute de tension) : broches en entrée, sorties à 0.
    void resetMcp(TwoWire& wire)
    {
        wire.registers[IODIRA] = 0xFF;
        wire.registers[IODIRB] = 0xFF;
        wire.registers[OLATA] = 0x00;
        wire.registers[OLATB] = 0x00;
    }

    AnalogRoute pt100Input(uint8_t channel)
    {
        return AnalogRoute::forInput(
            settingsFor(Type::Pt100, Wiring::FourWire), channel);
    }

    TEST_CASE("routage analogique : octets des ports")
    {
        for (const ExpectedRoute& expected : ROUTES)
        {
            const Sensor::Settings settings =
                settingsFor(expected.type, expected.wiring);

            const AnalogRoute idle = AnalogRoute::forInput(settings);
            CHECK_TRUE(idle.portA == expected.portA);
            CHECK_TRUE(idle.portB == 0);

            for (uint8_t channel = 0; channel < MAX_RTD; channel++)
            {
                const AnalogRoute route =
                    AnalogRoute::forInput(settings, channel);

                CHECK_TRUE(route.portA == expected.portA);
                CHECK_TRUE(route.portB == (1u << channel));
            }
        }
    }

    TEST_CASE("routage analogique : thermocouple en 2 fils")
    {
        const AnalogRoute twoWire =
            AnalogRoute::forInput(settingsFor(Type::Tc, Wiring::TwoWire), 1);

        CHECK_TRUE(AnalogRoute::forInput(
            settingsFor(Type::Tc, Wiring::ThreeWire), 1) == twoWire);
        CHECK_TRUE(AnalogRoute::forInput(
            settingsFor(Type::Tc, Wiring::FourWire), 1) == twoWire);
    }

    TEST_CASE("routage analogique : sans voie")
    {
        const Sensor::Settings settings =
            settingsFor(Type::Pt100, Wiring::FourWire);

        CHECK_TRUE(AnalogRoute::forInput(settings, MAX_RTD) ==
                   AnalogRoute::forInput(settings));
        CHECK_TRUE(AnalogRoute::forInput(
            settings, AnalogRoute::NO_CHANNEL).portB == 0);
    }

    TEST_CASE("routage analogique : broches en sortie")
    {
        const AnalogRoute outputs = AnalogRoute::outputPins();

        CHECK_TRUE(outputs.portA == 0xFF);
        CHECK_TRUE(outputs.portB == 0x07);

        // Tout routage reste dans les broches mises en sortie.
        for (const ExpectedRoute& expected : ROUTES)
        {
            for (uint8_t channel = 0; channel < MAX_RTD; channel++)
            {
                const AnalogRoute route = AnalogRoute::forInput(
                    settingsFor(expected.type, expected.wiring), channel);

                CHECK_TRUE((route.portA & ~outputs.portA) == 0);
                CHECK_TRUE((route.portB & ~outputs.portB) == 0);
            }
        }
    }

    TEST_CASE("MCP23017 : séquence d'initialisation")
    {
        // Redémarrage sans reset du MCP : sorties actives, voie 1 connectée.
        TwoWire wire = mcpBus();
        wire.registers[IODIRA] = 0x00;
        wire.registers[IODIRB] = 0xF8;
        wire.registers[OLATA] = 0x15;
        wire.registers[OLATB] = 0x01;

        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));
        CHECK_TRUE(wire.started);

        // IOCON (deux adresses), sorties à 0, puis directions.
        const std::vector<Bytes> expected = {
            {0x05, 0x00},
            {IOCON, 0x00},
            {OLATA, 0x00, 0x00},
            {IODIRA, 0x00, 0xF8},
        };

        CHECK_TRUE(wire.writeLog == expected);
        CHECK_TRUE(wire.registers[OLATA] == 0x00);
        CHECK_TRUE(wire.registers[OLATB] == 0x00);
        CHECK_TRUE(wire.registers[IODIRA] == 0x00);
        CHECK_TRUE(wire.registers[IODIRB] == 0xF8);
        CHECK_TRUE(wire.registers[IOCON] == 0x00);
    }

    TEST_CASE("MCP23017 : absent au démarrage")
    {
        TwoWire absent = mcpBus();
        absent.connected = false;

        AnalogMux mux;
        CHECK_FALSE(mux.begin(absent));
        CHECK_TRUE(absent.writeLog.empty());

        // Autre circuit à l'adresse attendue : pas de réponse du MCP.
        TwoWire otherDevice;
        otherDevice.deviceAddress = 0x68;

        AnalogMux otherMux;
        CHECK_FALSE(otherMux.begin(otherDevice));
    }

    TEST_CASE("MCP23017 : routage en une transaction")
    {
        TwoWire wire = mcpBus();

        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));
        wire.writeLog.clear();

        const AnalogRoute route = AnalogRoute::forInput(
            settingsFor(Type::Pt100, Wiring::FourWire), 1);

        CHECK_TRUE(mux.write(route));

        const std::vector<Bytes> expected = {{OLATA, 0x06, 0x02}};
        CHECK_TRUE(wire.writeLog == expected);
        CHECK_TRUE(wire.registers[OLATA] == 0x06);
        CHECK_TRUE(wire.registers[OLATB] == 0x02);
    }

    TEST_CASE("MCP23017 : écriture refusée")
    {
        TwoWire wire = mcpBus();

        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));

        // NACK : l'échec est signalé et les ports ne changent pas.
        wire.nextTransmissionError = 2;
        CHECK_FALSE(mux.write(AnalogRoute::forInput(
            settingsFor(Type::Pt1000, Wiring::TwoWire), 0)));
        CHECK_TRUE(wire.registers[OLATA] == 0x00);
        CHECK_TRUE(wire.registers[OLATB] == 0x00);

        // Sans begin(), aucun bus : rien n'est écrit.
        AnalogMux unstarted;
        CHECK_FALSE(unstarted.write(AnalogRoute{}));
    }
}

namespace
{
    TEST_CASE("MCP23017 : routage vérifié")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));
        wire.writeLog.clear();

        CHECK_TRUE(mux.apply(pt100Input(1)));

        // Écriture, puis relecture à partir de IODIRA.
        const std::vector<Bytes> expected = {{OLATA, 0x06, 0x02}, {IODIRA}};
        CHECK_TRUE(wire.writeLog == expected);
        CHECK_TRUE(mux.counters().busErrors == 0);
        CHECK_TRUE(mux.counters().reinitializations == 0);
        CHECK_TRUE(mux.counters().pinMismatches == 0);
        CHECK_TRUE(mux.confirm());
    }

    TEST_CASE("MCP23017 : NACK isolé rattrapé")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));

        // NACK isolé : la nouvelle écriture suffit.
        wire.nextTransmissionError = 2;
        CHECK_TRUE(mux.apply(pt100Input(0)));
        CHECK_TRUE(mux.counters().busErrors == 1);
        CHECK_TRUE(mux.counters().reinitializations == 0);
        CHECK_TRUE(wire.registers[OLATB] == 0x01);
    }

    TEST_CASE("MCP23017 : réinitialisé après un reset")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));

        // Broches en entrée : la réécriture seule ne suffit pas.
        resetMcp(wire);
        CHECK_TRUE(mux.apply(pt100Input(2)));
        CHECK_TRUE(mux.counters().reinitializations == 1);
        CHECK_TRUE(wire.registers[IODIRA] == 0x00);
        CHECK_TRUE(wire.registers[IODIRB] == 0xF8);
        CHECK_TRUE(wire.registers[OLATA] == 0x06);
        CHECK_TRUE(wire.registers[OLATB] == 0x04);
    }

    TEST_CASE("MCP23017 : muet puis de retour")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));

        wire.connected = false;
        CHECK_FALSE(mux.apply(pt100Input(0)));
        CHECK_TRUE(mux.counters().reinitializations == 1);
        CHECK_TRUE(mux.counters().busErrors > 0);

        // De retour : état inconnu, la réinitialisation passe en premier.
        wire.connected = true;
        wire.writeLog.clear();
        CHECK_TRUE(mux.apply(pt100Input(0)));
        CHECK_TRUE(!wire.writeLog.empty() &&
                   wire.writeLog.front() == Bytes({0x05, 0x00}));
        CHECK_TRUE(mux.counters().reinitializations == 2);
    }

    TEST_CASE("MCP23017 : reset pendant l'acquisition")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));
        CHECK_TRUE(mux.apply(pt100Input(0)));

        // Reset pendant l'acquisition : la mesure n'est pas prouvée. Les
        // broches en entrée ne sont pas comptées comme non conformes.
        resetMcp(wire);
        CHECK_FALSE(mux.confirm());
        CHECK_FALSE(mux.takePinMismatch());
        CHECK_TRUE(mux.counters().pinMismatches == 0);

        // Le routage suivant commence par la réinitialisation.
        wire.writeLog.clear();
        CHECK_TRUE(mux.apply(pt100Input(1)));
        CHECK_TRUE(!wire.writeLog.empty() &&
                   wire.writeLog.front() == Bytes({0x05, 0x00}));
        CHECK_TRUE(mux.counters().reinitializations == 1);
    }

    TEST_CASE("MCP23017 : relecture en fin d'acquisition")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));
        CHECK_TRUE(mux.apply(pt100Input(0)));

        // Lecture incomplète isolée : relue une fois.
        wire.shortRead = true;
        CHECK_TRUE(mux.confirm());
        CHECK_TRUE(mux.counters().busErrors == 1);

        // Bus muet : mesure non prouvée.
        wire.connected = false;
        CHECK_FALSE(mux.confirm());
    }

    TEST_CASE("MCP23017 : broche non conforme refusée")
    {
        TwoWire wire = mcpBus();
        AnalogMux mux;
        CHECK_TRUE(mux.begin(wire));
        CHECK_FALSE(mux.takePinMismatch());

        // Broche SW_4 (GPA1) bloquée à 0 : deux essais, la réinitialisation
        // et un dernier essai, tous refusés.
        wire.mcpStuckLowA = 0x02;
        CHECK_FALSE(mux.apply(pt100Input(0)));
        CHECK_TRUE(mux.counters().pinMismatches == 3);
        CHECK_TRUE(mux.counters().reinitializations == 1);
        CHECK_TRUE(mux.takePinMismatch());
        CHECK_FALSE(mux.takePinMismatch());

        // Broche libérée : le routage suivant passe.
        wire.mcpStuckLowA = 0;
        CHECK_TRUE(mux.apply(pt100Input(0)));
        CHECK_TRUE(mux.confirm());

        // Broche bloquée pendant l'acquisition : la mesure est rejetée.
        wire.mcpStuckLowA = 0x02;
        CHECK_FALSE(mux.confirm());
    }
}
