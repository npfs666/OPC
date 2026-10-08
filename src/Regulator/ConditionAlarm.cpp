// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/ConditionAlarm.h>

#include <Arduino.h>
#include <Inputs/DigitalInput.h>
#include <hmi/ParameterList.h>

void ConditionAlarm::begin(
    const char* key,
    const char* name)
{
    Regulator::begin(key, name);
    beginAlarm();
    input = nullptr;
    written = false;
    requested = false;
    missing = false;
    restart();
}

void ConditionAlarm::begin(
    const char* key,
    const char* name,
    const DigitalInput& input)
{
    begin(key, name);
    this->input = &input;
}

void ConditionAlarm::set(bool condition)
{
    written = true;
    requested = condition;
}

bool ConditionAlarm::isEnabled() const
{
    return settings.enabled;
}

void ConditionAlarm::restart()
{
    inputSeen = false;
    pending = false;
    confirmed = false;
    pendingSince = 0;
}

void ConditionAlarm::evaluate(bool condition, bool fault, uint32_t now)
{
    // Inhibée par la glue : la condition ne déclenche pas ; un défaut si.
    if (isInhibited() && !fault)
        condition = false;

    if (!condition)
    {
        pending = false;
        confirmed = false;
    }
    else
    {
        if (!pending)
        {
            pending = true;
            confirmed = false;
            pendingSince = now;
        }

        if (!confirmed &&
            now - pendingSince >= settings.delay * 1000UL)
        {
            confirmed = true;
        }
    }

    applyCondition(confirmed, settings.latching);
}

void ConditionAlarm::update(uint32_t now)
{
    if (input == nullptr)
        return;

    if (!settings.enabled)
    {
        restart();
        clearAlarm();
        return;
    }

    pollAcknowledgeInput();

    if (input->isValid())
    {
        inputSeen = true;
        evaluate(input->isActive(), false, now);
        return;
    }

    // Pas encore lue (démarrage, anti-rebond après le menu) : ni alarme ni
    // défaut. Invalide après avoir été lue : défaut.
    evaluate(inputSeen, inputSeen, now);
}

bool ConditionAlarm::applyLogic(uint32_t now)
{
    if (input != nullptr)
        return false;

    const bool wasWritten = written;
    written = false;

    if (!settings.enabled)
    {
        missing = false;
        restart();
        clearAlarm();
        return false;
    }

    pollAcknowledgeInput();

    if (wasWritten)
    {
        missing = false;
        evaluate(requested, false, now);
        return false;
    }

    // Condition non écrite : défaut, signalé et journalisé une fois.
    const bool started = !missing;
    missing = true;
    evaluate(true, true, now);
    return started;
}

void ConditionAlarm::resume(uint32_t now)
{
    // L'alarme survit à la reprise (Alarm) ; l'entrée doit être relue.
    inputSeen = false;
    Alarm::resume(now);
}

void ConditionAlarm::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "alarms",
        "Alarmes",
        getConfigurationKey(),
        getName()
    });

    parameters.addBool(
        "enabled",
        "Active",
        settings.enabled);

    parameters.addInteger(
        "delay",
        "Tempo",
        settings.delay,
        0,
        3600,
        5,
        "s");

    parameters.addBool(
        "latching",
        "Mémorisation",
        settings.latching);
}
