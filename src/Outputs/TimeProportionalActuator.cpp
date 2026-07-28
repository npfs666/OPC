#include <Outputs/TimeProportionalActuator.h>

TimeProportionalActuator::TimeProportionalActuator()
{
}

void TimeProportionalActuator::begin(
    const char* name,
    Regulator& regulator,
    uint32_t period)
{
    begin(
        name,
        name,
        regulator,
        period);
}

void TimeProportionalActuator::begin(
    const char* key,
    const char* name,
    Regulator& regulator,
    uint32_t period)
{
    Actuator::begin(key, name, regulator);

    settings.period =
        period == 0
            ? 1000
            : period;

    cycleStart = 0;

    relayState = false;
}

void TimeProportionalActuator::update(uint32_t now)
{
    if (regulator == nullptr ||
        !regulator->isCommandValid() ||
        settings.period == 0)
    {
        relayState = false;

        for (uint8_t i = 0;
             i < outputCount;
             i++)
        {
            outputs[i]->forceSafe();
        }

        return;
    }

    double_t command = regulator->readCommand();

    const uint32_t elapsedCycles =
        (now - cycleStart) /
        settings.period;

    cycleStart +=
        elapsedCycles *
        settings.period;

    uint32_t elapsed = now - cycleStart;

    bool state = elapsed < (uint32_t)(command * settings.period);

    relayState = state;

    for (uint8_t i = 0; i < outputCount; i++)
    {
        outputs[i]->setCommand(
            relayState ? 1.0 : 0.0,
            now);
    }
}

void TimeProportionalActuator::resume(uint32_t now)
{
    cycleStart = now;
    relayState = false;
}

void TimeProportionalActuator::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "actuators",
        "Actionneurs",
        getConfigurationKey(),
        getName()
    });

    parameters.addInteger(
        "period",
        "Période",
        settings.period,
        1000,
        3600000,
        1000,
        "ms");

    Actuator::registerParameters(list);
}
