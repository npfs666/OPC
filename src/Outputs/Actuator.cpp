#include <Outputs/Actuator.h>

Actuator::Actuator()
{
}

void Actuator::begin(const char* name, Regulator& regulator)
{
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

