#ifndef SOLARREGULATOR_H
#define SOLARREGULATOR_H

#include <Regulator/Regulator.h>

class Temperature;

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

    void resume(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor)
        const override;
    
    void print(Stream& stream) const override;

private:

    Temperature* collector = nullptr;

    Temperature* tankTop = nullptr;

    Temperature* tankBottom = nullptr;

    bool running = false;
};

#endif
