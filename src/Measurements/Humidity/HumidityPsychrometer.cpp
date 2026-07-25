#include <Measurements/Humidity/HumidityPsychrometer.h>

HumidityPsychrometer::HumidityPsychrometer()
{
}

void HumidityPsychrometer::begin(
    const char* name,
    const Psychrometer& psychrometer)
{
    Humidity::begin(name);
    this->psychrometer = &psychrometer;
}

void HumidityPsychrometer::update()
{
    setValue(psychrometer->relativeHumidity());

    setValid(true);
}