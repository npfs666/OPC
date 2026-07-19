#ifndef ONOFFACTUATOR_H
#define ONOFFACTUATOR_H

#include <Outputs/Actuator.h>

class ActuatorOnOff : public Actuator
{
public:

    ActuatorOnOff(const char* name);

    void update(uint32_t now) override;
};

#endif