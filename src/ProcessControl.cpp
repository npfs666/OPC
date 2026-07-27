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

    if (id >= measurementCount)
        return nullptr;

    return measurements[id];
}

/**
 * Only for testing hardware, do not correct
 */
void ProcessControl::printCSVPsychro(uint32_t now) {

    Serial.print(now/1000.0,1);
    Serial.print(" ; ");
    Serial.print(getMeasurement(4)->getValue());
    Serial.print(" ; ");
    Serial.println(getMeasurement(7)->getValue());
}

void ProcessControl::print(Stream& stream) const
{
    //stream.println();
    //stream.println(F("===== Process Control ====="));

    //stream.println(F("Measurements"));
    stream.println(F("--------------------------"));
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        measurements[i]->print(stream);
    }

    //double_t deltas = measurements[3]->getValue() - measurements[5]->getValue(); 
    //Serial.println(deltas, 3);
    
    if (regulatorCount > 0 )  {
        stream.println();
        stream.println(F("Regulators"));
        stream.println(F("--------------------------"));
    }
    
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

void ProcessControl::registerParameters(ParameterList& list)
{
    for (size_t i = 0; i < regulatorCount; i++)
    {
        regulators[i]->registerParameters(list);
    }

    for (size_t i = 0; i < actuatorCount; i++)
    {
        actuators[i]->registerParameters(list);
    }

    /*for (size_t i = 0; i < outputCount; i++)
    {
        outputs[i]->registerParameters(list);
    }*/

    /*for (size_t i = 0; i < measurementCount; i++)
    {
        measurements[i]->registerParameters(list);
    }*/
}