#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Regulator/Regulator.h>
#include <Measurements/Temperature/Temperature.h>

class Thermostat : public Regulator
{
public:

    struct Settings
    {
        double_t setpoint;
        double_t hysteresis;
    };

    Settings settings;

    Thermostat(
        const char* name,
        Temperature& temperature);

    void update(uint32_t now);

    void print(Stream& stream) const override;

private:

    Temperature& temperature;
};

#endif