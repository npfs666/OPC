#include <Outputs/Actuator.h>

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
    this->key = key;
    Displayable::begin(name);

    this->regulator = &regulator;

    outputCount = 0;
}

void Actuator::addOutput(Output& output)
{
    if (outputCount >= MAX_OUTPUTS)
        return;

    outputs[outputCount++] = &output;
}

void Actuator::resume(uint32_t now)
{
    (void)now;
}

const char* Actuator::getKey() const
{
    return key;
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
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr)
            outputs[i]->registerParameters(list);
    }
}
