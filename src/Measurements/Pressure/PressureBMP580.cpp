#include <Measurements/Pressure/PressureBMP580.h>

#include <Adafruit_BMP5xx.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_PRESSURE = 30000.0;
    constexpr double_t MAX_PRESSURE = 110000.0;
}

PressureBMP580::PressureBMP580()
{
}

void PressureBMP580::begin(
    const char* name,
    Adafruit_BMP5xx& bmp580)
{
    Pressure::begin(name);
    this->bmp580 = &bmp580;
}

double_t PressureBMP580::pressureSeaLevel(
    int16_t altitude)
{
    if (bmp580 == nullptr)
        return NAN;

    const double_t pressure =
        bmp580->readPressure() *
        100.0;

    if (!std::isfinite(pressure) ||
        pressure < MIN_PRESSURE ||
        pressure > MAX_PRESSURE)
    {
        return NAN;
    }

    const double_t temperature =
        bmp580->readTemperature();

    if (!std::isfinite(temperature))
        return NAN;

    return
        pressure /
        pow(
            1.0 -
                ((0.0065 * altitude) /
                 (temperature + 273.14)),
            5.255);
}

double_t PressureBMP580::printValue() const
{
    return getValue();
}

uint8_t PressureBMP580::printDecimals() const
{
    return 1;
}

void PressureBMP580::update()
{
    if (bmp580 == nullptr)
    {
        setValid(false);
        return;
    }

    const double_t value =
        bmp580->readPressure() *
        100.0;

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