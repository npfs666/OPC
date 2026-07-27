#include "Outputs/Output.h"

Output::Output()
{
}

void Output::begin(const char* name)
{
    begin(name, name);
}

void Output::begin(
    const char* key,
    const char* name)
{
    this->key = key;
    Displayable::begin(name);
    command = 0.0;
}

void Output::writeCommand(double_t value)
{
    command = constrain(value, 0.0, 1.0);
}

double_t Output::readCommand() const
{
    return command;
}

const char* Output::getKey() const
{
    return key;
}

double_t Output::printValue() const
{
    return command;
}

const char* Output::getUnit() const
{
    return "";
}
