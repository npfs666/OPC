#include "Outputs/Output.h"

#include <Arduino.h>

#include <cmath>

Output::Output()
{
}

void Output::begin(const char* name)
{
    begin(name, name);
}

void Output::begin(
    const char* key,
    const char* name)
{
    beginConfiguration(key);
    Displayable::begin(name);
    requested = 0.0;
    applied = 0.0;
    lastCommandTime = 0;
}

void Output::setCommand(
    double_t value,
    uint32_t now)
{
    if (!std::isfinite(value))
        value = 0.0;

    requested =
        constrain(value, 0.0, 1.0);

    lastCommandTime = now;
}

double_t Output::requestedCommand() const
{
    return requested;
}

double_t Output::appliedCommand() const
{
    return applied;
}

uint32_t Output::lastCommandAt() const
{
    return lastCommandTime;
}

void Output::setAppliedCommand(double_t value)
{
    applied = constrain(value, 0.0, 1.0);
}

double_t Output::printValue() const
{
    return applied;
}

const char* Output::getUnit() const
{
    return "";
}
