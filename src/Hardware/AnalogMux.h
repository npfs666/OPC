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
 *
 * Vérification par relecture de IODIR à OLAT. Après un reset (ESD, chute de
 * tension), le MCP repart avec ses broches en entrée et seul ce pilote le
 * reconfigure : un routage encore en place en fin d'acquisition prouve qu'il
 * a tenu pendant toute l'acquisition.
 */
class AnalogMux
{
public:
    // Depuis le démarrage.
    struct Counters
    {
        uint32_t busErrors = 0;         // transactions I²C en échec
        uint32_t reinitializations = 0; // réinitialisations complètes du MCP
        uint32_t pinMismatches = 0;     // broche relue différente de sa commande
    };

    // Retourne false si le MCP23017 ne répond pas : le routage est alors inconnu.
    bool begin(TwoWire& wire);

    /**
     * Écrit le routage complet, sans vérification (étapes intermédiaires,
     * arrêt). Pour changer de montage sans relier deux configurations à une
     * entrée, couper d'abord la voie (routage sans voie), puis la connecter.
     */
    bool write(const AnalogRoute& route);

    /**
     * Écrit le routage et le vérifie. En cas d'échec : une nouvelle écriture,
     * puis une réinitialisation complète du MCP et un dernier essai.
     * false : le routage n'est pas prouvé.
     */
    bool apply(const AnalogRoute& route);

    /**
     * Fin d'acquisition : le dernier routage écrit est-il toujours en place ?
     * Une lecture ratée est refaite une fois. false : la mesure n'est pas
     * prouvée, et le MCP sera réinitialisé au prochain apply().
     */
    bool confirm();

    // Vrai si une broche a été relue différente de sa commande depuis
    // l'appel précédent.
    bool takePinMismatch();

    const Counters& counters() const { return diagnostics; }

private:
    enum class Check : uint8_t
    {
        Ok,
        BusError,
        Mismatch
    };

    bool configure();
    bool writeAndVerify(const AnalogRoute& route);
    Check verify(const AnalogRoute& route);

    bool writeRegisters(
        uint8_t address,
        const uint8_t* values,
        uint8_t count);

    bool readRegisters(
        uint8_t address,
        uint8_t* values,
        uint8_t count);

    TwoWire* bus = nullptr;
    AnalogRoute current;
    bool reinitPending = false;
    bool pinMismatchSeen = false;
    Counters diagnostics;
};

#endif
