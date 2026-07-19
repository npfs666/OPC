#include <Outputs/Actuator.h>

Actuator::Actuator(const char* name)
{
    actuatorName = name;

    command = 0.0;

    outputCount = 0;
}

void Actuator::addOutput(Output& output)
{
    if (outputCount >= MAX_OUTPUTS)
        return;

    outputs[outputCount++] = &output;
}

void Actuator::writeCommand(double_t value)
{
    command = constrain(value, 0.0, 1.0);
}

double_t Actuator::readCommand() const
{
    return command;
}

const char* Actuator::readName() const
{
    return actuatorName;
}