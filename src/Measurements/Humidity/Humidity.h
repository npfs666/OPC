#ifndef HUMIDITY_H
#define HUMIDITY_H

#include <Measurements/Measurement.h>

class Humidity : public Measurement
{
public:
    Humidity() = default;

    void begin(const char* name)
    {
        Measurement::begin(name, "%");
    }

    virtual void update() = 0;
};

#endif