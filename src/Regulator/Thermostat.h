#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Regulator/Regulator.h>
#include <Regulator/SetpointRamp.h>

class Temperature;

class Thermostat : public Regulator
{
public:
    enum class Mode : uint8_t
    {
        Heating,
        Cooling
    };

    struct Settings
    {
        Mode mode = Mode::Heating;
        double_t setpoint = 20.0;
        double_t hysteresis = 1.0;
    };

    Settings settings;
    SetpointRamp setpointRamp;

    Thermostat();

    void begin(const char* name, Temperature& temperature);
    void begin(
        const char* key,
        const char* name,
        Temperature& temperature);

    void update(uint32_t now);
    void resume(uint32_t now) override;

    void print(Stream& stream) const override;

    void registerParameters(ParameterList& list) override;

private:

    Temperature* temperature = nullptr;
};

#endif
