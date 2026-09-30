#include "Outputs/RelayOutput.h"

RelayOutput::RelayOutput(
    const char *name,
    uint8_t pin,
    bool activeHigh)
    : Output(name)
{
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