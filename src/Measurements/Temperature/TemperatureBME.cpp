#include <Measurements/Temperature/TemperatureBME.h>

#include <Adafruit_BME280.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_TEMPERATURE = -40.0;
    constexpr double_t MAX_TEMPERATURE = 85.0;
}

TemperatureBME::TemperatureBME()
{
}


void TemperatureBME::begin(
    const char* name,
    Adafruit_BME280& bme)
{
    Temperature::begin(name);
    this->bme = &bme;
}

void TemperatureBME::update()
{
    if (bme == nullptr)
    {
        setValid(false);
        return;
    }

    const double_t value =
        bme->readTemperature();

    if (!std::isfinite(value) ||
        value < MIN_TEMPERATURE ||
        value > MAX_TEMPERATURE)
    {
        setValid(false);
        return;
    }

    setValue(value);
    setValid(true);
}
