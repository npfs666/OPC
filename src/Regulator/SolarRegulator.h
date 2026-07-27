#ifndef SOLARREGULATOR_H
#define SOLARREGULATOR_H

#include <Arduino.h>

#include <Measurements/Temperature/Temperature.h>
#include <Regulator/Regulator.h>

class SolarRegulator : public Regulator
{
public:

    struct Settings
    {
        double_t startDelta;
        double_t stopDelta;

        double_t maximumTankTemperature;

        double_t minimumCollectorTemperature;
    };

    Settings settings;

    SolarRegulator();

    void begin(const char* name,
        Temperature& collector,
        Temperature& tankTop,
        Temperature& tankBottom);

    void begin(
        const char* key,
        const char* name,
        Temperature& collector,
        Temperature& tankTop,
        Temperature& tankBottom);

    void update(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

private:

    Temperature* collector;

    Temperature* tankTop;

    Temperature* tankBottom;

    bool running;
};

#endif
