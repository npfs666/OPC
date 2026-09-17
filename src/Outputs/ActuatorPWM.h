#ifndef ACTUATORPWM_H
#define ACTUATORPWM_H

#include <Outputs/Actuator.h>

// Transmet la commande 0..1 du régulateur sans conversion tout-ou-rien.
class ActuatorPWM : public Actuator
{
public:
    using Actuator::begin;

    void update(uint32_t now) override;
};

#endif
