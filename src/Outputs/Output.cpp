// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Outputs/Output.h"

#include <Arduino.h>
#include <hmi/ParameterEditor.h>

#include <cmath>
#include <cstring>

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

bool Output::pinIsUnique(
    const ParameterEditor& editor,
    const char* ownerKey)
{
    const ParameterDraft* pin = editor.find(ownerKey, "pin");

    if (pin == nullptr)
        return false;

    // Deux sorties ne doivent pas piloter la même broche après édition.
    for (size_t i = 0; i < editor.count(); i++)
    {
        const ParameterDraft& other = editor.get(i);
        const Parameter* parameter = other.parameter;

        if (&other != pin && parameter != nullptr &&
            parameter->type == Parameter::Type::Selection &&
            std::strcmp(parameter->categoryKey, "outputs") == 0 &&
            std::strcmp(parameter->key, "pin") == 0 &&
            other.selectionValue == pin->selectionValue)
        {
            return false;
        }
    }

    return true;
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
