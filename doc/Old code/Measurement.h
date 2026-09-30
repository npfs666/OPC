#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <Arduino.h>

enum class MeasurementType : uint8_t
{
    Temperature,
    RelativeHumidity,
    DewPoint,
    AmosphericPressure,
};

enum class MeasurementID
{
    ChamberTemperature,
    ProductTemperature,
    RelativeHumidity,
    DewPoint,
    WetBulb,
    DryBulb
};

enum class MeasurementUnit : uint8_t {

    Temperature,
    Percent,
    Pascal,
    mBar
};

class Measurement
{
public:

    Measurement();

    Measurement(MeasurementType type, uint8_t rtd);
    Measurement(MeasurementType type, uint8_t dryRTD, uint8_t wetRTD);

    MeasurementType type;

    uint8_t input1;
    uint8_t input2;
    uint8_t input3;

    double_t value;

    const char* name;
    const char* unit;

    void updateTemperature(double_t temperature);

    void updateHumidity(double_t humidity);
};

#endif