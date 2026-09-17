#include <Outputs/ActuatorPWM.h>

#include <Outputs/Output.h>
#include <Regulator/Regulator.h>

void ActuatorPWM::update(uint32_t now)
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (regulator == nullptr || !regulator->isCommandValid())
            outputs[i]->forceSafe();
        else
            outputs[i]->setCommand(regulator->readCommand(), now);
    }
}
