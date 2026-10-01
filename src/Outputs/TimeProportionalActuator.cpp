#include <Outputs/TimeProportionalActuator.h>

#include <Outputs/Output.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

TimeProportionalActuator::TimeProportionalActuator()
{
}

void TimeProportionalActuator::begin(
    const char* name,
    Regulator& regulator,
    uint32_t period,
    uint32_t minPulse)
{
    begin(
        name,
        name,
        regulator,
        period,
        minPulse);
}

void TimeProportionalActuator::begin(
    const char* key,
    const char* name,
    Regulator& regulator,
    uint32_t period,
    uint32_t minPulse)
{
    Actuator::begin(key, name, regulator);

    settings.period =
        period == 0
            ? 1000
            : period;

    settings.minPulse =
        minPulse <= settings.period / 2
            ? minPulse
            : settings.period / 2;

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

    /*
     * Appliqué à chaque tour, sans figer la durée en début de période : une
     * commande à 0 ou 1 (autotune) bascule toujours immédiatement.
     */
    if (settings.minPulse > 0)
    {
        const uint32_t onTime =
            static_cast<uint32_t>(command * settings.period);

        if (onTime < settings.minPulse)
            command = 0.0;
        else if (settings.period - onTime < settings.minPulse)
            command = 1.0;
    }

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

    parameters.addInteger(
        "min_pulse",
        "Impulsion mini",
        settings.minPulse,
        0,
        60000,
        100,
        "ms");

    Actuator::registerParameters(list);
}

bool TimeProportionalActuator::validateParameters(
    const ParameterEditor& editor) const
{
    const ParameterDraft* period =
        editor.find(getConfigurationKey(), "period");

    const ParameterDraft* minPulse =
        editor.find(getConfigurationKey(), "min_pulse");

    if (period == nullptr || minPulse == nullptr)
        return true;

    return minPulse->integerValue <= period->integerValue / 2;
}
