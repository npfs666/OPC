#include <Regulator/Regulator.h>

Regulator::Regulator() 
{
}

void Regulator::begin(const char* name)
{
    begin(name, name);
}

void Regulator::begin(
    const char* key,
    const char* name)
{
    this->key = key;
    Displayable::begin(name);
    command = 0.0;
}

const char* Regulator::getKey() const
{
    return key;
}

void Regulator::writeCommand(double_t value)
{
    command = constrain(value, 0.0, 1.0);
}

double_t Regulator::readCommand() const
{
    return command;
}

double_t Regulator::printValue() const {
    return command;
}

const char* Regulator::getUnit() const {
    return "";
}
