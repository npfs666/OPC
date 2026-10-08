// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/Regulator.h>

#include <Arduino.h>
#include <Regulator/Alarm.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr ParameterOption FAULT_ACTION_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Regulator::FaultAction::SafeState),
            "Sécurité"
        },
        {
            static_cast<int32_t>(
                Regulator::FaultAction::Hold),
            "Maintien"
        }
    };
}

Regulator::Regulator() 
{
}

void Regulator::begin(const char* name)
{
    begin(name, name);
}

void Regulator::begin(
    const char* key,
    const char* name)
{
    beginConfiguration(key);
    Displayable::begin(name);
    command = 0.0;
    commandValid = false;
    inhibitRequested = false;
}

void Regulator::resume(uint32_t now)
{
    (void)now;
    invalidateCommand();
}

void Regulator::writeCommand(double_t value)
{
    command = constrain(value, 0.0, 1.0);
    commandValid = true;
    faultActive = false;
    holding = false;
}

void Regulator::invalidateCommand()
{
    command = 0.0;
    commandValid = false;
    faultActive = false;
    holding = false;
}

void Regulator::handleMeasurementFault(
    uint32_t now,
    MeasurementStatus status)
{
    // Début du défaut : la commande en cours est celle à maintenir.
    if (!faultActive)
    {
        faultActive = true;
        faultStart = now;
        holdAvailable = commandValid;
        holdCommand = command;
    }

    const bool hold =
        faultSettings.action == FaultAction::Hold &&
        status != MeasurementStatus::NotReady &&
        holdAvailable &&
        now - faultStart < faultSettings.holdTime * 1000UL;

    holding = hold;
    command = hold ? holdCommand : 0.0;
    commandValid = hold;
}

bool Regulator::isInFallback() const
{
    return holding && commandValid;
}

void Regulator::lockFaultAction(FaultAction action)
{
    faultActionLocked = true;
    faultSettings.action = action;
}

void Regulator::registerFaultParameters(ParameterList& list)
{
    if (faultActionLocked)
        return;

    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    });

    parameters.addSelection(
        "fault_action",
        "Si défaut",
        faultSettings.action,
        FAULT_ACTION_OPTIONS);

    parameters.addInteger(
        "fault_hold_time",
        "Maintien max",
        faultSettings.holdTime,
        5,
        600,
        5,
        "s");
}

double_t Regulator::readCommand() const
{
    // Inhibition d'un régulateur seulement : une alarme inhibée garde sa
    // commande (mémorisation), voir Alarm::inhibit().
    return Regulator::isInhibited() ? 0.0 : command;
}

bool Regulator::isCommandValid() const
{
    return (Regulator::isInhibited() || commandValid) && !isInterlocked();
}

void Regulator::inhibit(bool inhibited)
{
    inhibitRequested = inhibited;
}

bool Regulator::isInhibited() const
{
    return inhibitRequested && !isManual();
}

void Regulator::setInterlock(const Alarm& alarm)
{
    interlock = &alarm;
}

bool Regulator::isInterlocked() const
{
    return interlock != nullptr && interlock->locksOutputs();
}

double_t Regulator::printValue() const {
    return readCommand();
}

const char* Regulator::getUnit() const {
    return "";
}
