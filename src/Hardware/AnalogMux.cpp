// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/AnalogMux.h>

#include <Hardware/pinout.h>

namespace
{
    // Registres du MCP23017 avec IOCON.BANK = 0 (ports A et B entrelacés).
    constexpr uint8_t REG_IODIRA = 0x00;
    constexpr uint8_t REG_GPINTENB = 0x05; // IOCON si BANK = 1
    constexpr uint8_t REG_IOCON = 0x0A;
    constexpr uint8_t REG_OLATA = 0x14;

    // BANK = 0, adresses séquentielles (SEQOP = 0), interruptions inutilisées.
    constexpr uint8_t IOCON_VALUE = 0x00;
}

bool AnalogMux::begin(TwoWire& wire)
{
    bus = &wire;

    // Sans effet si le bus est déjà démarré (horloge).
    wire.begin();

    const AnalogRoute outputs = AnalogRoute::outputPins();
    const AnalogRoute off;

    const uint8_t latches[2] = {off.portA, off.portB};

    // IODIR = 1 : entrée. Les broches inutilisées restent en entrée.
    const uint8_t directions[2] = {
        static_cast<uint8_t>(~outputs.portA),
        static_cast<uint8_t>(~outputs.portB)};

    /*
     * IOCON est écrit deux fois. À 0x05, il remet BANK à 0 si le MCP était en
     * BANK = 1 ; en BANK = 0, 0x05 est GPINTENB, qui reste à 0. Les sorties
     * sont ensuite mises à 0 avant d'être activées : après un redémarrage
     * sans reset du MCP, aucune broche ne repasse en sortie avec son ancien
     * état. La première écriture sert aussi de test de présence.
     */
    return writeRegisters(REG_GPINTENB, &IOCON_VALUE, 1) &&
           writeRegisters(REG_IOCON, &IOCON_VALUE, 1) &&
           writeRegisters(REG_OLATA, latches, 2) &&
           writeRegisters(REG_IODIRA, directions, 2);
}

bool AnalogMux::write(const AnalogRoute& route)
{
    const uint8_t latches[2] = {route.portA, route.portB};

    return writeRegisters(REG_OLATA, latches, 2);
}

bool AnalogMux::writeRegisters(
    uint8_t address,
    const uint8_t* values,
    uint8_t count)
{
    if (bus == nullptr)
        return false;

    bus->beginTransmission(Board::Mcp23017::ADDRESS);
    bus->write(address);
    bus->write(values, count);

    return bus->endTransmission() == 0;
}
