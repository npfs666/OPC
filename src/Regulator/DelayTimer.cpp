// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/DelayTimer.h>

#include <Arduino.h>
#include <hmi/ParameterList.h>

#include <cstdint>

namespace
{
    constexpr uint32_t SECOND_MS = 1000UL;
    constexpr uint32_t MINUTE_MS = 60UL * SECOND_MS;
    constexpr uint32_t HOUR_MS = 60UL * MINUTE_MS;

    // Durées comparées à now - changedAt : moins de la moitié de la plage
    // de millis(), sans ambiguïté au débordement.
    constexpr uint32_t MAX_DELAY_MS = INT32_MAX;

    const char* unitText(DelayTimer::Unit unit)
    {
        switch (unit)
        {
        case DelayTimer::Unit::Minutes:
            return "min";
        case DelayTimer::Unit::Hours:
            return "h";
        default:
            return "s";
        }
    }
}

void DelayTimer::begin(
    const char* key,
    const char* name,
    Mode mode,
    uint32_t delay,
    Unit unit)
{
    Regulator::begin(key, name);

    this->mode = mode;
    this->unit = unit;
    settings.delay = delay;

    switch (unit)
    {
    case Unit::Minutes:
        maximum = 1440;
        break;
    case Unit::Hours:
        maximum = 48;
        break;
    default:
        maximum = 3600;
        break;
    }

    source = nullptr;
    label = "Délai";
    menuParent = nullptr;

    reset();
}

void DelayTimer::setSource(const Regulator& source)
{
    this->source = &source;
}

void DelayTimer::setMaximum(uint32_t maximum)
{
    const uint32_t limit = MAX_DELAY_MS / unitMs();
    this->maximum = maximum < limit ? maximum : limit;
}

void DelayTimer::setLabel(const char* label)
{
    this->label = label;
}

void DelayTimer::setMenuParent(const char* ownerKey)
{
    menuParent = ownerKey;
}

uint32_t DelayTimer::unitMs() const
{
    switch (unit)
    {
    case Unit::Minutes:
        return MINUTE_MS;
    case Unit::Hours:
        return HOUR_MS;
    default:
        return SECOND_MS;
    }
}

uint32_t DelayTimer::delayMs() const
{
    const uint32_t limit = MAX_DELAY_MS / unitMs();
    const uint32_t delay =
        settings.delay < limit ? settings.delay : limit;

    return delay * unitMs();
}

void DelayTimer::reset()
{
    started = false;
    input = false;
    changedAt = 0;
    armed = false;
    output = false;
}

bool DelayTimer::run(bool input, uint32_t now)
{
    // Inhibée : arrêt commandé ; à la levée, la temporisation repart de zéro.
    if (isInhibited())
    {
        reset();
        invalidateCommand();
        return false;
    }

    if (!started || input != this->input)
    {
        started = true;
        this->input = input;
        changedAt = now;
    }

    /*
     * Un délai écoulé reste acquis (output, armed) : now - changedAt
     * reboucle après 49 jours et ne doit plus être consulté ensuite.
     */
    const bool elapsed = now - changedAt >= delayMs();

    if (mode == Mode::OnDelay)
    {
        output = input && (output || elapsed);
    }
    else
    {
        if (input)
            armed = true;
        else if (armed && elapsed)
            armed = false;

        output = input || armed;
    }

    writeCommand(output ? 1.0 : 0.0);
    return output;
}

bool DelayTimer::isOn() const
{
    return isCommandValid() && output;
}

uint32_t DelayTimer::remainingMs(uint32_t now) const
{
    if (!started)
        return 0;

    // Un délai ne court que vers le changement de la sortie : montée en
    // retard à la montée, retombée en retard à la descente.
    const bool timing =
        mode == Mode::OnDelay
            ? input && !output
            : !input && output;

    if (!timing)
        return 0;

    const uint32_t elapsed = now - changedAt;
    const uint32_t delay = delayMs();

    return elapsed < delay ? delay - elapsed : 0;
}

void DelayTimer::update(uint32_t now)
{
    if (source == nullptr)
        return;

    if (isInhibited() || !source->isCommandValid())
    {
        reset();
        invalidateCommand();
        return;
    }

    run(source->readCommand() >= 0.5, now);
}

void DelayTimer::resume(uint32_t now)
{
    // Le délai en cours continue : seule la commande attend le prochain
    // calcul.
    Regulator::resume(now);
}

void DelayTimer::registerParameters(ParameterList& list)
{
    ParameterOwner owner{
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    };

    owner.parentOwnerKey = menuParent;

    auto parameters = list.forOwner(owner);

    parameters.addInteger(
        "delay",
        label,
        settings.delay,
        0,
        static_cast<int32_t>(maximum),
        1,
        unitText(unit));
}
