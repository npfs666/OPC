// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Outputs/RelayOutput.h"

#include <Arduino.h>
#include <Hardware/pinout.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <climits>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr ParameterOption RELAY_PIN_OPTIONS[] = {
        {
            Board::Rp2040::OUTPUT_1,
            "Relais 1"
        },
        {
            Board::Rp2040::OUTPUT_2,
            "Relais 2"
        }
    };
}

RelayOutput::RelayOutput()
{
}



void RelayOutput::begin(
    const char* name,
    uint8_t pin,
    bool activeHigh,
    bool safeState)
{
    begin(
        name,
        name,
        pin,
        activeHigh,
        safeState);
}

void RelayOutput::begin(
    const char* key,
    const char* name,
    uint8_t pin,
    bool activeHigh,
    bool safeState)
{
    Output::begin(key, name);

    std::snprintf(
        counterOwnerKey,
        sizeof(counterOwnerKey),
        "%s.counters",
        key);
    
    settings.pin = pin;
    settings.activeHigh = activeHigh;
    settings.safeState = safeState;
}

bool RelayOutput::begin()
{
    if (safeStateLocked)
        settings.safeState = lockedSafeState;

    // Une nouvelle application des réglages ne relance pas les temps
    // minimaux ; une nouvelle broche repart comme au démarrage.
    const bool newPin =
        !initialized ||
        configuredPin != settings.pin;

    if (newPin)
    {
        lastSwitchTime = millis();
        setAppliedCommand(0.0);
    }

    if (initialized)
    {
        writePhysicalState(
            configuredPin,
            configuredSafeState,
            configuredActiveHigh);
    }

    pinMode(settings.pin, OUTPUT);

    configuredPin = settings.pin;
    configuredActiveHigh =
        settings.activeHigh;
    configuredSafeState =
        settings.safeState;

    initialized = true;

    forceSafe();

    return true;
}

void RelayOutput::poll(uint32_t now)
{
    (void)now;

    if (!initialized)
        return;

    const bool requestedState =
        requestedCommand() >= 0.5;

    const bool appliedState =
        appliedCommand() >= 0.5;

    refreshCounterDisplay();

    if (requestedState == appliedState ||
        !minimumTimeElapsed(appliedState))
        return;

    applyLogicalState(requestedState);
}

const OutputCounters* RelayOutput::counters() const
{
    return &counterData;
}

double_t RelayOutput::onSeconds() const
{
    double_t total = counterData.onSeconds;

    if (initialized && appliedCommand() >= 0.5)
        total += (millis() - onSpanStart) / 1000.0;

    return total;
}

void RelayOutput::resetCounters()
{
    counterData.switches = 0;
    counterData.onSeconds = 0.0;
    onSpanStart = millis();
    refreshCounterDisplay();
}

void RelayOutput::restoreCounters(uint32_t switches, double_t onSeconds)
{
    counterData.switches = switches;
    counterData.onSeconds =
        std::isfinite(onSeconds) && onSeconds > 0.0 ? onSeconds : 0.0;
    refreshCounterDisplay();
}

void RelayOutput::refreshCounterDisplay()
{
    displayedSwitches = counterData.switches;
    displayedOnHours = onSeconds() / 3600.0;
}

bool RelayOutput::minimumTimeElapsed(bool appliedState) const
{
    const uint32_t minimumMs =
        (appliedState
            ? settings.minOnTime
            : settings.minOffTime) * 1000UL;

    return millis() - lastSwitchTime >= minimumMs;
}

bool RelayOutput::isWaiting() const
{
    return
        initialized &&
        (requestedCommand() >= 0.5) !=
            (appliedCommand() >= 0.5);
}

void RelayOutput::forceSafe()
{
    if (safeStateLocked)
        settings.safeState = lockedSafeState;

    const double_t safeCommand =
        settings.safeState ? 1.0 : 0.0;

    requested = safeCommand;

    if (!initialized)
    {
        setAppliedCommand(safeCommand);
        return;
    }

    applyLogicalState(settings.safeState);
}

double_t RelayOutput::safeCommand() const
{
    const bool safeState =
        safeStateLocked
            ? lockedSafeState
            : settings.safeState;

    return safeState ? 1.0 : 0.0;
}

void RelayOutput::lockSafeCommand(double_t safeCommand)
{
    lockSafeState(safeCommand >= 0.5);
}

bool RelayOutput::applySettings()
{
    if (safeStateLocked)
        settings.safeState = lockedSafeState;

    return begin();
}

bool RelayOutput::isHealthy() const
{
    return initialized;
}

void RelayOutput::lockSafeState(bool safeState)
{
    safeStateLocked = true;
    lockedSafeState = safeState;
    settings.safeState = safeState;

    if (initialized)
        forceSafe();
}

void RelayOutput::applyLogicalState(bool state)
{
    const bool wasOn = appliedCommand() >= 0.5;

    if (state != wasOn)
    {
        const uint32_t now = millis();

        // Compteurs d'entretien : chaque basculement réel, et la durée de
        // la marche qui se termine.
        if (initialized)
        {
            if (wasOn)
                counterData.onSeconds += (now - onSpanStart) / 1000.0;

            if (counterData.switches < UINT32_MAX)
                counterData.switches++;
        }

        if (state)
            onSpanStart = now;

        lastSwitchTime = now;
    }

    writePhysicalState(
        configuredPin,
        state,
        configuredActiveHigh);

    setAppliedCommand(
        state ? 1.0 : 0.0);
}

void RelayOutput::writePhysicalState(
    uint8_t pin,
    bool logicalState,
    bool activeHigh)
{
    const bool physicalLevel =
        activeHigh
            ? logicalState
            : !logicalState;

    digitalWrite(pin, physicalLevel);
}

void RelayOutput::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "outputs",
        "Sorties",
        getConfigurationKey(),
        getName()
    });

    parameters.addSelection(
        "pin",
        "Broche",
        settings.pin,
        RELAY_PIN_OPTIONS);

    parameters.addBool(
        "active_high",
        "Actif à HIGH",
        settings.activeHigh);

    parameters.addBool(
        "safe_state",
        "État de sécurité",
        settings.safeState,
        safeStateLocked);

    parameters.addInteger(
        "min_on_time",
        "Marche mini",
        settings.minOnTime,
        0,
        1800,
        5,
        "s");

    parameters.addInteger(
        "min_off_time",
        "Arrêt mini",
        settings.minOffTime,
        0,
        1800,
        5,
        "s");

    // Divers > Compteurs > <relais> : valeurs en lecture seule (sauvegardées
    // à part, dans /counters.json) et seuil d'entretien.
    auto counterValues = list.forOwner({
        "miscellaneous",
        "Divers",
        counterOwnerKey,
        getName(),
        false,
        "counters"
    });

    counterValues.addInteger(
        "switches",
        "Manœuvres",
        displayedSwitches,
        0,
        INT32_MAX,
        1,
        nullptr,
        true);

    counterValues.addDouble(
        "on_hours",
        "Heures ON",
        displayedOnHours,
        "h",
        true,
        1);

    auto counterSettings = list.forOwner({
        "miscellaneous",
        "Divers",
        counterOwnerKey,
        getName(),
        true,
        "counters"
    });

    // Un relais standard tient environ 150 000 manœuvres à pleine charge.
    counterSettings.addInteger(
        "maintenance_limit",
        "Seuil entret.",
        counterData.maintenanceLimit,
        0,
        2000000,
        10000);

    // Sans effet sur la régulation : appliqué sans état sûr.
    list.setLive(counterOwnerKey, "maintenance_limit");
}

bool RelayOutput::validateParameters(
    const ParameterEditor& editor) const
{
    if (!pinIsUnique(editor, getConfigurationKey()))
        return false;

    if (!safeStateLocked)
        return true;

    const ParameterDraft* safeState =
        editor.find(
            getConfigurationKey(),
            "safe_state");

    return
        safeState != nullptr &&
        safeState->parameter != nullptr &&
        safeState->parameter->type ==
            Parameter::Type::Bool &&
        safeState->booleanValue ==
            lockedSafeState;
}
