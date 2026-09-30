#include "Measurement.h"

Measurement::Measurement()
{
    type = MeasurementType::Temperature;

    input1 = 0;
    input2 = 0;

    value = NAN;

    name = "";
    unit = "";
}

Measurement::Measurement(MeasurementType type, uint8_t rtd)
{
    this->type = type;

    input1 = rtd;
    input2 = 0;

    value = NAN;

    name = "Temperature";
    unit = "°C";
}

Measurement::Measurement(MeasurementType type, uint8_t dryRTD, uint8_t wetRTD)
{
    this->type = type;

    input1 = dryRTD;
    input2 = wetRTD;

    value = NAN;

    name = "Humidity";
    unit = "%RH";
}

void Measurement::updateTemperature(double_t temperature)
{
    value = temperature;
}

void Measurement::updateHumidity(double_t humidity)
{
    value = humidity;
}