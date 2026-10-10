// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/AnalogMux.h>

#include <Hardware/pinout.h>

namespace
{
    // Registres du MCP23017 avec IOCON.BANK = 0 (ports A et B entrelacés).
    constexpr uint8_t REG_IODIRA = 0x00;
    constexpr uint8_t REG_IODIRB = 0x01;
    constexpr uint8_t REG_GPINTENB = 0x05; // IOCON si BANK = 1
    constexpr uint8_t REG_IOCON = 0x0A;
    constexpr uint8_t REG_GPIOA = 0x12;
    constexpr uint8_t REG_GPIOB = 0x13;
    constexpr uint8_t REG_OLATA = 0x14;
    constexpr uint8_t REG_OLATB = 0x15;
    constexpr uint8_t REGISTER_COUNT = 0x16;

    // BANK = 0, adresses séquentielles (SEQOP = 0), interruptions inutilisées.
    constexpr uint8_t IOCON_VALUE = 0x00;

    /*
     * Une broche relue différente de sa commande (court-circuit, charge) est
     * seulement journalisée, le temps de valider ce contrôle sur la carte :
     * une broche chargée donnerait sinon un défaut permanent.
     */
    constexpr bool PIN_MISMATCH_IS_FAULT = false;
}

bool AnalogMux::begin(TwoWire& wire)
{
    bus = &wire;
    reinitPending = false;

    // Sans effet si le bus est déjà démarré (horloge).
    wire.begin();

    return configure();
}

bool AnalogMux::configure()
{
    const AnalogRoute outputs = AnalogRoute::outputPins();
    const AnalogRoute off;

    const uint8_t latches[2] = {off.portA, off.portB};

    // IODIR = 1 : entrée. Les broches inutilisées restent en entrée.
    const uint8_t directions[2] = {
        static_cast<uint8_t>(~outputs.portA),
        static_cast<uint8_t>(~outputs.portB)};

    current = off;

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
    current = route;

    const uint8_t latches[2] = {route.portA, route.portB};

    return writeRegisters(REG_OLATA, latches, 2);
}

bool AnalogMux::apply(const AnalogRoute& route)
{
    // MCP dans un état inconnu : directement la réinitialisation.
    if (!reinitPending)
    {
        for (uint8_t attempt = 0; attempt < 2; attempt++)
        {
            if (writeAndVerify(route))
                return true;
        }
    }

    diagnostics.reinitializations++;
    reinitPending = !(configure() && writeAndVerify(route));

    return !reinitPending;
}

bool AnalogMux::confirm()
{
    Check check = verify(current);

    if (check == Check::BusError)
        check = verify(current);

    if (check != Check::Ok)
        reinitPending = true;

    return check == Check::Ok;
}

bool AnalogMux::takePinMismatch()
{
    const bool seen = pinMismatchSeen;
    pinMismatchSeen = false;
    return seen;
}

bool AnalogMux::writeAndVerify(const AnalogRoute& route)
{
    return write(route) && verify(route) == Check::Ok;
}

AnalogMux::Check AnalogMux::verify(const AnalogRoute& route)
{
    uint8_t registers[REGISTER_COUNT] = {};

    if (!readRegisters(REG_IODIRA, registers, REGISTER_COUNT))
        return Check::BusError;

    const AnalogRoute outputs = AnalogRoute::outputPins();

    const bool inPlace =
        (registers[REG_IODIRA] & outputs.portA) == 0 &&
        (registers[REG_IODIRB] & outputs.portB) == 0 &&
        registers[REG_IOCON] == IOCON_VALUE &&
        registers[REG_OLATA] == route.portA &&
        registers[REG_OLATB] == route.portB;

    // Configuration en place, mais broche relue différente de sa commande :
    // court-circuit ou charge. Après un reset, les broches sont en entrée et
    // leur niveau n'apprend rien de plus que IODIR.
    const bool pinMismatch =
        inPlace &&
        (((registers[REG_GPIOA] ^ route.portA) & outputs.portA) != 0 ||
         ((registers[REG_GPIOB] ^ route.portB) & outputs.portB) != 0);

    if (pinMismatch)
    {
        diagnostics.pinMismatches++;
        pinMismatchSeen = true;
    }

    if (!inPlace || (PIN_MISMATCH_IS_FAULT && pinMismatch))
        return Check::Mismatch;

    return Check::Ok;
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

    if (bus->endTransmission() == 0)
        return true;

    diagnostics.busErrors++;
    return false;
}

bool AnalogMux::readRegisters(
    uint8_t address,
    uint8_t* values,
    uint8_t count)
{
    if (bus == nullptr)
        return false;

    bus->beginTransmission(Board::Mcp23017::ADDRESS);
    bus->write(address);

    if (bus->endTransmission(false) != 0 ||
        bus->requestFrom(Board::Mcp23017::ADDRESS, count, true) != count)
    {
        diagnostics.busErrors++;
        return false;
    }

    for (uint8_t i = 0; i < count; i++)
        values[i] = static_cast<uint8_t>(bus->read());

    return true;
}
