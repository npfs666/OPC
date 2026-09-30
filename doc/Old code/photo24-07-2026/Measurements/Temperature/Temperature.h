#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Measurements/Measurement.h>

class Temperature : public Measurement
{
public:
    Temperature(const char* name)
        : Measurement(name, "°C")
    {
    }

    virtual void update() = 0;
};

#endif