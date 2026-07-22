#include "Outputs/Output.h"

Output::Output(const char *name) : Displayable(name)
{
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