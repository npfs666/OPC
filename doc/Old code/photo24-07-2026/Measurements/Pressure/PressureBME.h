#ifndef BMEPRESSURE_H
#define BMEPRESSURE_H

#include <Measurements/Pressure/Pressure.h>

#include <Adafruit_BME280.h>

class PressureBME : public Pressure
{
public:

    PressureBME(const char* name,
                Adafruit_BME280& bme);

    double_t pressureSeaLevel(int16_t altitude);

    void update() override;

    double_t printValue() const override;

    uint8_t printDecimals() const override;

private:

    Adafruit_BME280& bme;
};

#endif