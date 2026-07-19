#include <Outputs/ActuatorOnOff.h>

ActuatorOnOff::ActuatorOnOff(const char* name)
    : Actuator(name)
{

}

void ActuatorOnOff::update(uint32_t now)
{
    double_t outputCommand;

    if (command >= 0.5)
        outputCommand = 1.0;
    else
        outputCommand = 0.0;

    for (uint8_t i = 0; i < outputCount; i++)
        outputs[i]->writeCommand(outputCommand);
}