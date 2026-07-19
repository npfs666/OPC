#ifndef PSYCHROMETER_H
#define PSYCHROMETER_H

#include <Measurements/Temperature/Temperature.h>
#include <Measurements/Pressure/Pressure.h>

class Psychrometer
{
public:

    Psychrometer(const Temperature& dryBulb,
                 const Temperature& wetBulb,
                 const Pressure& pressure);

    double_t relativeHumidity() const;

    double_t absoluteHumidity() const;

    double_t dewPoint() const;

private:

    const Temperature& dryBulb;
    const Temperature& wetBulb;
    const Pressure& pressure;
};

#endif