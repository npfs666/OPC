#include <Measurements/Humidity/HumidityPsychrometer.h>

HumidityPsychrometer::HumidityPsychrometer(
    const char* name,
    const Psychrometer& psychrometer)
    : Humidity(name),
      psychrometer(psychrometer)
{
}

void HumidityPsychrometer::update()
{
    setValue(psychrometer.relativeHumidity());

    setValid(true);
}