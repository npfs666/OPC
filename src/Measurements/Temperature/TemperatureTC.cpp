#include <Measurements/Temperature/TemperatureTC.h>
#include <Hardware/SensorBoard.h>

#include <cmath>

void TemperatureTC::begin(const char* name, Sensor& sensor)
{
    Temperature::begin(name);
    this->sensor = &sensor;
    setValue(NAN);
    setValid(false);
}

void TemperatureTC::update()
{
    setValid(false);
    setValue(NAN);

    if (sensor == nullptr || sensor->getBoard() == nullptr ||
        sensor->settings.type != Sensor::Type::Tc)
        return;

    const auto& board = *sensor->getBoard();
    const double temperature = Physics::Thermocouple::compensatedTemperature(
        sensor->settings.thermocoupleType,
        board.computeVoltage(*sensor),
        board.getColdJunctionTemperature()) + sensor->settings.offset;

    if (!std::isfinite(temperature))
        return;

    setValue(temperature);
    setValid(true);
}
