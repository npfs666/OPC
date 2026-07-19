#ifndef TIMEPROPORTIONALACTUATOR_H
#define TIMEPROPORTIONALACTUATOR_H

#include <Outputs/Actuator.h>

class TimeProportionalActuator : public Actuator
{
public:

    struct Settings
    {
        uint32_t period;
    };

    Settings settings;

    TimeProportionalActuator(
        const char* name,
        uint32_t period);

    void update(uint32_t now) override;

private:

    uint32_t cycleStart;

    bool relayState;
};

#endif