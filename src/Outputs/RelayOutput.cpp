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
    bool activeHigh)
{
    begin(
        name,
        name,
        pin,
        activeHigh);
}

void RelayOutput::begin(
    const char* key,
    const char* name,
    uint8_t pin,
    bool activeHigh)
{
    Output::begin(key, name);
    
    settings.pin = pin;
    settings.activeHigh = activeHigh;
}

void RelayOutput::begin()
{
    pinMode(settings.pin, OUTPUT);
    writeCommand(0.0);
}

void RelayOutput::writeCommand(double_t value)
{
    Output::writeCommand(value);

    bool level = (command >= 0.5);

    if (!settings.activeHigh)
        level = !level;

    digitalWrite(settings.pin, level);
}

void RelayOutput::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "outputs",
        "Sorties",
        getKey(),
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
}
