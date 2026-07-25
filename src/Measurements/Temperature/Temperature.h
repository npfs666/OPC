#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Measurements/Measurement.h>

class Temperature : public Measurement
{
public:
    Temperature() = default;

    void begin(const char* name) {
        Measurement::begin(name, "°C");
    }

    virtual void update() = 0;
};

#endif