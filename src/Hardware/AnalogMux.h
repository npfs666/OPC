// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANALOGMUX_H
#define ANALOGMUX_H

#include <cstdint>
#include <Wire.h>
#include <Hardware/AnalogRoute.h>

/**
 * Multiplexeurs analogiques de la carte, commandés par le MCP23017 :
 *  - un devant l'ADC pour le montage 2, 3 ou 4 fils et la référence ;
 *  - un par voie d'entrée (MAX_RTD).
 *
 * Pilote par registres : chaque écriture donne l'état complet des deux
 * ports (OLATA puis OLATB) en une transaction. Il n'y a plus de
 * lecture-modification-écriture : une lecture ratée ne peut plus recopier
 * un port entier à 0xFF.
 */
class AnalogMux
{
public:
    // Retourne false si le MCP23017 ne répond pas : le routage est alors inconnu.
    bool begin(TwoWire& wire);

    /**
     * Écrit le routage complet. Pour changer de montage sans relier deux
     * configurations à une entrée, couper d'abord la voie (routage sans
     * voie), puis la connecter. false si la transaction a échoué.
     */
    bool write(const AnalogRoute& route);

private:
    bool writeRegisters(
        uint8_t address,
        const uint8_t* values,
        uint8_t count);

    TwoWire* bus = nullptr;
};

#endif
