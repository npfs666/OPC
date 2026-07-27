#include <Regulator/Thermostat.h>

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
    
    settings.setpoint = 20.0;
    settings.hysteresis = 1.0;
}

void Thermostat::update(uint32_t now)
{
    (void)now;
    
    double_t value = temperature->getValue();

    if (value <= settings.setpoint - settings.hysteresis / 2.0)
    {
        writeCommand(1.0);
    }
    else if (value >= settings.setpoint + settings.hysteresis / 2.0)
    {
        writeCommand(0.0);
    }
}

void Thermostat::print(Stream& stream) const {

    PrintSize ps;

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
        getKey(),
        getName()
    });

    parameters.addDouble(
        "setpoint",
        "Consigne",
        settings.setpoint,
        0.0,
        80.0,
        0.5,
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
