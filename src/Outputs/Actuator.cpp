#include <Outputs/Actuator.h>

Actuator::Actuator(const char* name, Regulator& regulator):regulator(regulator), Displayable(name)
{
    outputCount = 0;
}

void Actuator::addOutput(Output& output)
{
    if (outputCount >= MAX_OUTPUTS)
        return;

    outputs[outputCount++] = &output;
}

