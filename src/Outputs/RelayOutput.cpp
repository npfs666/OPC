#include "Outputs/RelayOutput.h"

#include <Hardware/pinout.h>

namespace
{
    constexpr ParameterOption RELAY_PIN_OPTIONS[] = {
        {
            RELAIS_1,
            "Relais 1"
        },
        {
            RELAIS_2,
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
    
    settings.pin = pin;
    settings.activeHigh = activeHigh;
    settings.safeState = safeState;
}

bool RelayOutput::begin()
{
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

    if (requestedState == appliedState)
        return;

    applyLogicalState(requestedState);
}

void RelayOutput::forceSafe()
{
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

bool RelayOutput::applySettings()
{
    return begin();
}

bool RelayOutput::isHealthy() const
{
    return initialized;
}

void RelayOutput::applyLogicalState(bool state)
{
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
        settings.safeState);
}
