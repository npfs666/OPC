#include <Outputs/TimeProportionalActuator.h>

TimeProportionalActuator::TimeProportionalActuator(
    const char* name,
    Regulator& regulator,
    uint32_t period)
    : Actuator(name, regulator)
{
    settings.period = period;

    cycleStart = 0;

    relayState = false;
}

void TimeProportionalActuator::update(uint32_t now)
{
    double_t command = regulator.readCommand();

    while (now - cycleStart >= settings.period)
        cycleStart += settings.period;

    uint32_t elapsed = now - cycleStart;

    bool state = elapsed < (uint32_t)(command * settings.period);

    if (state == relayState)
        return;

    relayState = state;

    for (uint8_t i = 0; i < outputCount; i++)
        outputs[i]->writeCommand(relayState);
}