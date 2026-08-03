#include <ProcessControl.h>

#include <Measurements/Measurement.h>
#include <Outputs/Actuator.h>
#include <Outputs/Output.h>
#include <ProcessSnapshot.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterList.h>

ProcessControl::ProcessControl()
{
    measurementCount = 0;
    regulatorCount = 0;
    actuatorCount = 0;
    outputCount = 0;
}

bool ProcessControl::add(Measurement& measurement)
{
    if (measurementCount >= MAX_MEASUREMENTS)
        return false;

    for (uint8_t i = 0;
         i < measurementCount;
         i++)
    {
        if (measurements[i] == &measurement)
            return false;
    }

    measurements[measurementCount++] = &measurement;

    return true;
}

bool ProcessControl::add(Regulator& regulator)
{
    if (regulatorCount >= MAX_REGULATORS)
        return false;

    for (uint8_t i = 0;
         i < regulatorCount;
         i++)
    {
        if (regulators[i] == &regulator)
            return false;
    }

    regulators[regulatorCount++] = &regulator;

    return true;
}

bool ProcessControl::add(Actuator& actuator)
{
    if (actuatorCount >= MAX_ACTUATORS)
        return false;

    for (uint8_t i = 0;
         i < actuatorCount;
         i++)
    {
        if (actuators[i] == &actuator)
            return false;
    }

    actuators[actuatorCount++] = &actuator;

    return true;
}

bool ProcessControl::connect(
    Actuator& actuator,
    Output& output)
{
    bool actuatorRegistered = false;

    for (uint8_t i = 0;
         i < actuatorCount;
         i++)
    {
        if (actuators[i] == &actuator)
        {
            actuatorRegistered = true;
            break;
        }
    }

    if (!actuatorRegistered)
        return false;

    if (outputCount >= MAX_REGISTERED_OUTPUTS)
        return false;

    for (uint8_t i = 0;
         i < outputCount;
         i++)
    {
        if (outputs[i] == &output)
            return false;
    }

    if (!actuator.addOutput(output))
        return false;

    outputs[outputCount++] = &output;

    return true;
}

void ProcessControl::updateMeasurementsAndRegulators(
    uint32_t now)
{
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        if (measurements[i] != nullptr)
            measurements[i]->update();
    }

    for (uint8_t i = 0; i < regulatorCount; i++)
        regulators[i]->update(now);

    poll(now);
}

void ProcessControl::poll(uint32_t now)
{
    for (uint8_t i = 0; i < actuatorCount; i++)
        actuators[i]->update(now);

    for (uint8_t i = 0; i < outputCount; i++)
        outputs[i]->poll(now);
}

void ProcessControl::resume(uint32_t now)
{
    for (uint8_t i = 0; i < regulatorCount; i++)
        regulators[i]->resume(now);

    for (uint8_t i = 0; i < actuatorCount; i++)
        actuators[i]->resume(now);
}

bool ProcessControl::beginOutputs()
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] == nullptr ||
            !outputs[i]->begin())
        {
            forceSafeOutputs();
            return false;
        }
    }

    forceSafeOutputs();

    return true;
}

bool ProcessControl::applyOutputSettings()
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] == nullptr ||
            !outputs[i]->applySettings())
        {
            forceSafeOutputs();
            return false;
        }
    }

    forceSafeOutputs();

    return true;
}

void ProcessControl::forceSafeOutputs()
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr)
            outputs[i]->forceSafe();
    }
}

bool ProcessControl::outputsHealthy() const
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] == nullptr ||
            !outputs[i]->isHealthy())
        {
            return false;
        }
    }

    return true;
}

void ProcessControl::captureSnapshot(
    ProcessSnapshot& destination,
    uint32_t now) const
{
    destination.clear(now);

    for (uint8_t i = 0; i < measurementCount; i++)
    {
        if (measurements[i] != nullptr)
            destination.add(*measurements[i]);
    }

    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr)
            destination.add(*outputs[i]);
    }
}

/*Measurement* ProcessControl::getMeasurement(uint8_t id) {

    if (id >= measurementCount)
        return nullptr;

    return measurements[id];
}*/

/**
 * Only for testing hardware, do not correct
 */
void ProcessControl::printCSVPsychro(Stream& stream) const {

    stream.print(measurements[4]->getValue());
    stream.print(";");
    stream.println(measurements[7]->getValue());
}

void ProcessControl::print(Stream& stream) const
{
    //stream.println();
    stream.println(F("===== Process Control ====="));

    stream.println(F("-------Measurements-------"));
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        measurements[i]->print(stream);
    }
    
    if (regulatorCount > 0 )  {
        stream.println(F("--------Regulators--------"));
    }
    
    for (uint8_t i = 0; i < regulatorCount; i++)
    {
        regulators[i]->print(stream);
    }

    if (actuatorCount > 0 )  {
        stream.println(F("--------Actuators---------"));
    }
    for (uint8_t i = 0; i < actuatorCount; i++)
    {
        actuators[i]->print(stream);
    }

    if (outputCount > 0)
    {
        stream.println(F("----------Outputs---------"));
    }

    for (uint8_t i = 0; i < outputCount; i++)
    {
        outputs[i]->print(stream);
    }

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

    for (size_t i = 0; i < outputCount; i++)
    {
        outputs[i]->registerParameters(list);
    }

    /*for (size_t i = 0; i < measurementCount; i++)
    {
        measurements[i]->registerParameters(list);
    }*/
}

bool ProcessControl::validateParameters(
    const ParameterEditor& editor) const
{
    for (size_t i = 0; i < regulatorCount; i++)
    {
        if (regulators[i] != nullptr &&
            !regulators[i]->validateParameters(
                editor))
        {
            return false;
        }
    }

    for (size_t i = 0; i < actuatorCount; i++)
    {
        if (actuators[i] != nullptr &&
            !actuators[i]->validateParameters(
                editor))
        {
            return false;
        }
    }

    for (size_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr &&
            !outputs[i]->validateParameters(
                editor))
        {
            return false;
        }
    }

    return true;
}
