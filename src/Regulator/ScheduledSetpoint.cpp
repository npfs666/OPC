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

bool ScheduledSetpoint::update(
    double_t setpoint,
    double_t& target)
{
    target = setpoint;

    if (schedule == nullptr)
    {
        currentState = State::Unscheduled;
        return true;
    }

    bool active = false;

    if (!schedule->isActive(active))
    {
        currentState = State::ClockInvalid;
        return false;
    }

    if (active)
    {
        currentState = State::Comfort;
        return true;
    }

    if (settings.outside == Outside::Off)
    {
        currentState = State::Off;
        return false;
    }

    currentState = State::Reduced;
    target = settings.reducedSetpoint;

    return true;
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
