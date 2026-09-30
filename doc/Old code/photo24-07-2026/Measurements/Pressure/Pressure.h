#ifndef PRESSURE_H
#define PRESSURE_H

#include <Measurements/Measurement.h>

class Pressure : public Measurement
{
public:
    Pressure(const char* name)
        : Measurement(name, "Pa")
    {
    }

    virtual void update() = 0;
};

#endif