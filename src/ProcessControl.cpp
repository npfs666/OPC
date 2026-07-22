#include <ProcessControl.h>

ProcessControl::ProcessControl()
{
    measurementCount = 0;
    regulatorCount = 0;
    actuatorCount = 0;
}

void ProcessControl::add(Measurement& measurement)
{
    if (measurementCount >= MAX_MEASUREMENTS)
        return;

    measurements[measurementCount++] = &measurement;
}

void ProcessControl::add(Regulator& regulator)
{
    if (regulatorCount >= MAX_REGULATORS)
        return;

    regulators[regulatorCount++] = &regulator;
}

void ProcessControl::add(Actuator& actuator)
{
    if (actuatorCount >= MAX_ACTUATORS)
        return;

    actuators[actuatorCount++] = &actuator;
}

void ProcessControl::update(uint32_t now)
{
    for (uint8_t i = 0; i < measurementCount; i++)
        measurements[i]->update();

    for (uint8_t i = 0; i < regulatorCount; i++)
        regulators[i]->update(now);

    for (uint8_t i = 0; i < actuatorCount; i++)
        actuators[i]->update(now);
}

Measurement* ProcessControl::getMeasurement(uint8_t id) {

    return measurements[id];
}

void ProcessControl::print(Stream& stream) const
{
    stream.println();
    stream.println(F("===== Process Control ====="));

    stream.println(F("Measurements"));
    stream.println(F("--------------------------"));
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        measurements[i]->print(stream);
    }

    stream.println();

    stream.println(F("Regulators"));
    stream.println(F("--------------------------"));
    for (uint8_t i = 0; i < regulatorCount; i++)
    {
        regulators[i]->print(stream);
    }

    stream.println();
    /*
    stream.println(F("Actuators"));
    stream.println(F("--------------------------"));
    for (uint8_t i = 0; i < actuatorCount; i++)
    {
        actuators[i]->print(stream);
    }*/

    stream.println(F("=========================="));
    stream.println();
}