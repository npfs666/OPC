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
        return wire;
    }

    void testRouteTable()
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

    void testThermocoupleAlwaysTwoWire()
    {
        const AnalogRoute twoWire =
            AnalogRoute::forInput(settingsFor(Type::Tc, Wiring::TwoWire), 1);

        CHECK_TRUE(AnalogRoute::forInput(
            settingsFor(Type::Tc, Wiring::ThreeWire), 1) == twoWire);
        CHECK_TRUE(AnalogRoute::forInput(
            settingsFor(Type::Tc, Wiring::FourWire), 1) == twoWire);
    }

    void testNoChannelOutsideBoard()
    {
        const Sensor::Settings settings =
            settingsFor(Type::Pt100, Wiring::FourWire);

        CHECK_TRUE(AnalogRoute::forInput(settings, MAX_RTD) ==
                   AnalogRoute::forInput(settings));
        CHECK_TRUE(AnalogRoute::forInput(
            settings, AnalogRoute::NO_CHANNEL).portB == 0);
    }

    void testOutputPins()
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

    void testBeginSequence()
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

    void testBeginWithoutDevice()
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

    void testWriteIsOneTransaction()
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

    void testWriteFailure()
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

void runAnalogMuxTests()
{
    TestHarness::run("routage analogique : octets des ports", testRouteTable);
    TestHarness::run("routage analogique : thermocouple en 2 fils",
        testThermocoupleAlwaysTwoWire);
    TestHarness::run("routage analogique : sans voie",
        testNoChannelOutsideBoard);
    TestHarness::run("routage analogique : broches en sortie", testOutputPins);
    TestHarness::run("MCP23017 : séquence d'initialisation", testBeginSequence);
    TestHarness::run("MCP23017 : absent au démarrage", testBeginWithoutDevice);
    TestHarness::run("MCP23017 : routage en une transaction",
        testWriteIsOneTransaction);
    TestHarness::run("MCP23017 : écriture refusée", testWriteFailure);
}
