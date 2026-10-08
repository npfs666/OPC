// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/ScheduledSetpoint.h>

#include <Regulator/TimeSchedule.h>

namespace
{
    constexpr ParameterOption OUTSIDE_OPTIONS[] = {
        {
            static_cast<int32_t>(ScheduledSetpoint::Outside::Reduced),
            "Réduite"
        },
        {
            static_cast<int32_t>(ScheduledSetpoint::Outside::Off),
            "Arrêt"
        }
    };

    // Hors plage en arrêt ou heure inconnue : pas de consigne, régulation
    // arrêtée.
    bool regulates(ScheduledSetpoint::State state)
    {
        return state != ScheduledSetpoint::State::Off &&
               state != ScheduledSetpoint::State::ClockInvalid;
    }
}

void ScheduledSetpoint::begin()
{
    schedule = nullptr;
    settings = Settings{};
    currentState = State::Unscheduled;
}

void ScheduledSetpoint::attach(
    const TimeSchedule& schedule,
    double_t reducedSetpoint)
{
    this->schedule = &schedule;
    settings.reducedSetpoint = reducedSetpoint;
}

bool ScheduledSetpoint::isAttached() const
{
    return schedule != nullptr;
}

ScheduledSetpoint::State ScheduledSetpoint::evaluate(
    double_t setpoint,
    double_t& target) const
{
    target = setpoint;

    if (schedule == nullptr)
        return State::Unscheduled;

    bool active = false;

    if (!schedule->isActive(active))
        return State::ClockInvalid;

    if (active)
        return State::Comfort;

    if (settings.outside == Outside::Off)
        return State::Off;

    target = settings.reducedSetpoint;
    return State::Reduced;
}

bool ScheduledSetpoint::update(
    double_t setpoint,
    double_t& target)
{
    currentState = evaluate(setpoint, target);
    return regulates(currentState);
}

bool ScheduledSetpoint::readTarget(
    double_t setpoint,
    double_t& target) const
{
    return regulates(evaluate(setpoint, target));
}

ScheduledSetpoint::State ScheduledSetpoint::state() const
{
    return currentState;
}

const char* ScheduledSetpoint::stateName(State state)
{
    switch (state)
    {
    case State::Comfort:
        return "Confort";

    case State::Reduced:
        return "Réduit";

    case State::Off:
        return "Arrêt";

    case State::ClockInvalid:
        return "Heure invalide";

    default:
        return "";
    }
}

bool ScheduledSetpoint::registerParameters(
    ParameterList::Writer& parameters,
    double_t minimum,
    double_t maximum,
    double_t step,
    uint8_t decimals,
    const char* unit)
{
    if (schedule == nullptr)
        return true;

    return
        parameters.addDouble(
            "reduced_setpoint",
            "Cons. réduite",
            settings.reducedSetpoint,
            minimum,
            maximum,
            step,
            decimals,
            unit) &&
        parameters.addSelection(
            "outside_schedule",
            "Hors plage",
            settings.outside,
            OUTSIDE_OPTIONS);
}
