#include <Measurements/Humidity/HumidityBME.h>

#include <Adafruit_BME280.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_HUMIDITY = 0.0;
    constexpr double_t MAX_HUMIDITY = 100.0;
}

HumidityBME::HumidityBME()
{
}

void HumidityBME::begin(
    const char* name,
    Adafruit_BME280& bme)
{
    Humidity::begin(name);
    this->bme = &bme;
}

void HumidityBME::update()
{
    if (bme == nullptr)
    {
        setValid(false);
        return;
    }

    const double_t value =
        bme->readHumidity();

    if (!std::isfinite(value) ||
        value < MIN_HUMIDITY ||
        value > MAX_HUMIDITY)
    {
        setValid(false);
        return;
    }

    setValue(value);
    setValid(true);
}
