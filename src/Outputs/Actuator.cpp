#include <Outputs/Actuator.h>

#include <Outputs/Output.h>
#include <Regulator/Regulator.h>

Actuator::Actuator()
{
}

void Actuator::begin(const char* name, Regulator& regulator)
{
    begin(name, name, regulator);
}

void Actuator::begin(
    const char* key,
    const char* name,
    Regulator& regulator)
{
    beginConfiguration(key);
    Displayable::begin(name);

    this->regulator = &regulator;

    outputCount = 0;
}

bool Actuator::addOutput(Output& output)
{
    if (outputCount >= MAX_OUTPUTS)
        return false;

    for (uint8_t i = 0;
         i < outputCount;
         i++)
    {
        if (outputs[i] == &output)
            return false;
    }

    outputs[outputCount++] = &output;

    return true;
}

void Actuator::resume(uint32_t now)
{
    (void)now;
}

double_t Actuator::printValue() const
{
    if (regulator == nullptr)
        return 0.0;

    return regulator->readCommand();
}

const char* Actuator::getUnit() const
{
    return "";
}

void Actuator::registerParameters(
    ParameterList& list)
{
    (void)list;
}
