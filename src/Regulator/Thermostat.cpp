#include <Regulator/Thermostat.h>

#include <Arduino.h>
#include <Measurements/Temperature/Temperature.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    constexpr ParameterOption THERMOSTAT_MODE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Thermostat::Mode::Heating),
            "Chauffage"
        },
        {
            static_cast<int32_t>(
                Thermostat::Mode::Cooling),
            "Refroidissement"
        }
    };
}

Thermostat::Thermostat()
{
}

void Thermostat::begin(const char* name,Temperature& temperature)
{
    begin(name, name, temperature);
}

void Thermostat::begin(
    const char* key,
    const char* name,
    Temperature& temperature)
{
    Regulator::begin(key, name);

    this->temperature = &temperature;

    settings.mode = Mode::Heating;
    settings.setpoint = 20.0;
    settings.hysteresis = 1.0;
}

void Thermostat::update(uint32_t now)
{
    (void)now;

    if (temperature == nullptr ||
        !temperature->isValid() ||
        !std::isfinite(
            temperature->getValue()))
    {
        invalidateCommand();
        return;
    }

    const double_t value =
        temperature->getValue();

    const double_t lowerThreshold =
        settings.setpoint -
        settings.hysteresis / 2.0;

    const double_t upperThreshold =
        settings.setpoint +
        settings.hysteresis / 2.0;

    switch (settings.mode)
    {
    case Mode::Heating:
        if (value <= lowerThreshold)
            writeCommand(1.0);
        else if (value >= upperThreshold)
            writeCommand(0.0);
        break;

    case Mode::Cooling:
        if (value >= upperThreshold)
            writeCommand(1.0);
        else if (value <= lowerThreshold)
            writeCommand(0.0);
        break;

    default:
        invalidateCommand();
        break;
    }
}

void Thermostat::print(Stream& stream) const {
    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");

    stream.print((command == 0) ? "Off" : "On");
    stream.print(" | SP : ");
    stream.print(settings.setpoint);
    stream.print(" | Hyst : ");
    stream.print(settings.hysteresis);

    stream.println(' ');
}

void Thermostat::registerParameters(ParameterList& list) {

    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    });

    parameters.addSelection(
        "mode",
        "Mode",
        settings.mode,
        THERMOSTAT_MODE_OPTIONS);

    parameters.addDouble(
        "setpoint",
        "Consigne",
        settings.setpoint,
        0.0,
        200.0,
        0.1,
        1,
        "°C");

    parameters.addDouble(
        "hysteresis",
        "Hystérésis",
        settings.hysteresis,
        0.1,
        10.0,
        0.1,
        1,
        "°C");
}
