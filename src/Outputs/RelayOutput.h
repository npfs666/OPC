// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RELAYOUTPUT_H
#define RELAYOUTPUT_H

#include "Outputs/Output.h"

class RelayOutput : public Output
{
public:

    struct Settings
    {
        uint8_t pin = 0;
        bool activeHigh = true;
        bool safeState = false;

        /*
         * Temps minimaux en secondes (0 = sans contrainte), comptés depuis le
         * dernier basculement réel. Le passage à l'état sûr n'attend pas.
         */
        uint32_t minOnTime = 0;
        uint32_t minOffTime = 0;
    };

    Settings settings;

    RelayOutput();

    void begin(
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        bool safeState = false);

    void begin(
        const char* key,
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        bool safeState = false);

    bool begin() override;

    void poll(uint32_t now) override;

    void forceSafe() override;

    double_t safeCommand() const override;

    /** lockSafeState(safeCommand >= 0.5). */
    void lockSafeCommand(double_t safeCommand) override;

    bool applySettings() override;

    bool isHealthy() const override;

    bool isWaiting() const override;

    const OutputCounters* counters() const override;
    double_t onSeconds() const override;
    void resetCounters() override;
    void restoreCounters(uint32_t switches, double_t onSeconds) override;

    /** Clé du sous-menu Divers > Compteurs > <relais>. */
    const char* countersOwnerKey() const override
    {
        return counterOwnerKey;
    }

    /**
     * Impose l'état logique de sécurité et interdit sa restauration/édition.
     * Utile lorsqu'un état ON ne peut jamais être considéré comme sûr.
     */
    void lockSafeState(bool safeState = false);

    void registerParameters(
        ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor)
        const override;

private:
    void applyLogicalState(bool state);

    // Temps minimal de l'état appliqué écoulé (horloge millis()).
    bool minimumTimeElapsed(bool appliedState) const;

    static void writePhysicalState(
        uint8_t pin,
        bool logicalState,
        bool activeHigh);

    bool initialized = false;
    uint8_t configuredPin = 0;
    bool configuredActiveHigh = true;
    bool configuredSafeState = false;

    // Date (millis()) du dernier basculement, ou de la mise en service : au
    // démarrage, l'arrêt minimal s'applique avant la première mise en marche.
    uint32_t lastSwitchTime = 0;

    // Compteurs et copies affichées dans le menu (cœur contrôle).
    OutputCounters counterData;
    uint32_t onSpanStart = 0;         // début de la marche en cours
    uint32_t displayedSwitches = 0;
    double_t displayedOnHours = 0.0;
    char counterOwnerKey[40] = {};

    void refreshCounterDisplay();

    bool safeStateLocked = false;
    bool lockedSafeState = false;
};

#endif
