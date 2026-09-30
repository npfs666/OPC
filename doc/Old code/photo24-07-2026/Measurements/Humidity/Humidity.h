#ifndef HUMIDITY_H
#define HUMIDITY_H

#include <Measurements/Measurement.h>

class Humidity : public Measurement
{
public:

    Humidity(const char* name)
        : Measurement(name, "%")
    {
    }

    virtual void update() = 0;
};

#endif