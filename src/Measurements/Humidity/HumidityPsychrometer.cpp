#include <Measurements/Humidity/HumidityPsychrometer.h>

#include <Measurements/Psychrometer.h>

#include <cmath>

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
    if (psychrometer == nullptr)
    {
        setValid(false);
        return;
    }

    const MeasurementStatus inputStatus =
        psychrometer->status();

    if (inputStatus != MeasurementStatus::Ok)
    {
        setStatus(inputStatus);
        return;
    }

    const double_t humidity =
        psychrometer->relativeHumidity();

    if (!std::isfinite(humidity))
    {
        setValid(false);
        return;
    }

    setValue(humidity);
    setValid(true);
}
