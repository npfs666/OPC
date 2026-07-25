#ifndef PRESSURE_H
#define PRESSURE_H

#include <Measurements/Measurement.h>

class Pressure : public Measurement
{
public:
    Pressure() = default;

    void begin(const char* name)
    {
        Measurement::begin(name, "Pa");
    }

    virtual void update() = 0;
};

#endif