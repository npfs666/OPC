#include <Measurements/Pressure/PressureBME.h>

#include <Adafruit_BME280.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_TEMPERATURE = -40.0;
    constexpr double_t MAX_TEMPERATURE = 85.0;

    constexpr double_t MIN_PRESSURE = 30000.0;
    constexpr double_t MAX_PRESSURE = 110000.0;
}

PressureBME::PressureBME()
{
}

void PressureBME::begin(
    const char* name,
    Adafruit_BME280& bme)
{
    Pressure::begin(name);
    this->bme = &bme;
}

double_t PressureBME::pressureSeaLevel(int16_t altitude) {

    if (bme == nullptr)
        return NAN;

    const double_t pressure =
        bme->readPressure();

    const double_t temperature =
        bme->readTemperature();

    if (!std::isfinite(pressure) ||
        pressure < MIN_PRESSURE ||
        pressure > MAX_PRESSURE ||
        !std::isfinite(temperature) ||
        temperature < MIN_TEMPERATURE ||
        temperature > MAX_TEMPERATURE)
    {
        return NAN;
    }

    return
        pressure /
        pow(
            1.0 -
                ((0.0065 * altitude) /
                 (temperature + 273.14)),
            5.255);
}

double_t PressureBME::printValue() const {
    return getValue() / 100.0;
}

uint8_t PressureBME::printDecimals() const {
    return 1;
}

void PressureBME::update()
{
    if (bme == nullptr)
    {
        setValid(false);
        return;
    }

    const double_t value =
        bme->readPressure();

    if (!std::isfinite(value) ||
        value < MIN_PRESSURE ||
        value > MAX_PRESSURE)
    {
        setValid(false);
        return;
    }

    setValue(value);
    setValid(true);
}
