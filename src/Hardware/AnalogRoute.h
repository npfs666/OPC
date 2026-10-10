// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANALOGROUTE_H
#define ANALOGROUTE_H

#include <cstdint>
#include <Hardware/Sensor.h>

/**
 * Routage analogique de la carte : état complet des deux ports du MCP23017
 * (OLATA, OLATB). Sur la carte v0.2, le port A choisit le montage (2, 3 ou
 * 4 fils, thermocouple) et la résistance de référence, le port B la voie
 * d'entrée (voir Pinout_v0.2.h). AnalogRoute{} coupe tout.
 */
struct AnalogRoute
{
    // Aucune voie : le montage est préparé, l'entrée reste déconnectée.
    static constexpr uint8_t NO_CHANNEL = 0xFF;

    uint8_t portA = 0;
    uint8_t portB = 0;

    /**
     * Routage d'une entrée. Sans voie (NO_CHANNEL, ou voie absente de la
     * carte), aucune entrée n'est connectée. Un thermocouple se mesure
     * toujours en 2 fils.
     */
    static AnalogRoute forInput(
        const Sensor::Settings& settings,
        uint8_t channel = NO_CHANNEL);

    // Toutes les broches du routage, à mettre en sortie.
    static AnalogRoute outputPins();

    bool operator==(const AnalogRoute& other) const
    {
        return portA == other.portA && portB == other.portB;
    }

    bool operator!=(const AnalogRoute& other) const
    {
        return !(*this == other);
    }
};

#endif
