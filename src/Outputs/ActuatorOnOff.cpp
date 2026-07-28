#include <Outputs/ActuatorOnOff.h>

ActuatorOnOff::ActuatorOnOff()
{

}

void ActuatorOnOff::begin(const char* name, Regulator& regulator)
{
    begin(name, name, regulator);
}

void ActuatorOnOff::begin(
    const char* key,
    const char* name,
    Regulator& regulator)
{
    Actuator::begin(key, name, regulator);
}

void ActuatorOnOff::update(uint32_t now)
{
    if (regulator == nullptr ||
        !regulator->isCommandValid())
    {
        for (uint8_t i = 0;
             i < outputCount;
             i++)
        {
            outputs[i]->forceSafe();
        }

        return;
    }

    double_t outputCommand;

    double_t command = regulator->readCommand();

    if (command >= 0.5)
        outputCommand = 1.0;
    else
        outputCommand = 0.0;

    for (uint8_t i = 0; i < outputCount; i++)
    {
        outputs[i]->setCommand(
            outputCommand,
            now);
    }
}
