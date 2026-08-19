#include <Regulator/SetpointRamp.h>

#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    constexpr double_t MINIMUM_RATE = 0.1;
    constexpr double_t MAXIMUM_RATE = 20.0;
}

void SetpointRamp::begin()
{
    settings = Settings{};
    restart();
}

bool SetpointRamp::settingsAreValid() const
{
    return
        std::isfinite(settings.risingRate) &&
        std::isfinite(settings.fallingRate) &&
        settings.risingRate >= MINIMUM_RATE &&
        settings.risingRate <= MAXIMUM_RATE &&
        settings.fallingRate >= MINIMUM_RATE &&
        settings.fallingRate <= MAXIMUM_RATE;
}

bool SetpointRamp::update(
    uint32_t now,
    double_t target,
    double_t processValue)
{
    if (!std::isfinite(target) ||
        !std::isfinite(processValue) ||
        !settingsAreValid())
    {
        resume(now);
        activeValid = false;
        return false;
    }

    if (!settings.enabled)
    {
        active = target;
        previousTime = now;
        initialized = false;
        activeValid = true;
        return true;
    }

    if (!initialized)
    {
        active = processValue;
        previousTime = now;
        initialized = true;
        activeValid = true;
        return true;
    }

    const double_t elapsedMinutes =
        static_cast<double_t>(
            now - previousTime) /
        60000.0;

    previousTime = now;

    if (elapsedMinutes <= 0.0)
        return true;

    const double_t difference =
        target - active;

    const double_t rate =
        difference >= 0.0
            ? settings.risingRate
            : settings.fallingRate;

    const double_t maximumStep =
        rate * elapsedMinutes;

    if (std::fabs(difference) <= maximumStep)
    {
        active = target;
    }
    else
    {
        active +=
            difference > 0.0
                ? maximumStep
                : -maximumStep;
    }

    activeValid = std::isfinite(active);
    return activeValid;
}

void SetpointRamp::resume(uint32_t now)
{
    if (initialized)
        previousTime = now;
}

void SetpointRamp::restart()
{
    active = 0.0;
    previousTime = 0;
    initialized = false;
    activeValid = false;
}

double_t SetpointRamp::activeSetpoint() const
{
    return active;
}

bool SetpointRamp::hasActiveSetpoint() const
{
    return activeValid;
}

bool SetpointRamp::registerParameters(
    ParameterList& list,
    const char* ownerKey,
    const char* ownerName,
    const char* unit)
{
    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        ownerKey,
        ownerName
    });

    return
        parameters.addBool(
            "enabled",
            "Activée",
            settings.enabled) &&
        parameters.addDouble(
            "rising_rate",
            "Vitesse montée",
            settings.risingRate,
            MINIMUM_RATE,
            MAXIMUM_RATE,
            0.1,
            1,
            unit) &&
        parameters.addDouble(
            "falling_rate",
            "Vitesse descente",
            settings.fallingRate,
            MINIMUM_RATE,
            MAXIMUM_RATE,
            0.1,
            1,
            unit);
}
