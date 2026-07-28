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
    beginConfiguration(key);
    Displayable::begin(name);
    command = 0.0;
    commandValid = false;
}

void Regulator::resume(uint32_t now)
{
    (void)now;
    invalidateCommand();
}

void Regulator::writeCommand(double_t value)
{
    command = constrain(value, 0.0, 1.0);
    commandValid = true;
}

void Regulator::invalidateCommand()
{
    command = 0.0;
    commandValid = false;
}

double_t Regulator::readCommand() const
{
    return command;
}

bool Regulator::isCommandValid() const
{
    return commandValid;
}

double_t Regulator::printValue() const {
    return command;
}

const char* Regulator::getUnit() const {
    return "";
}
