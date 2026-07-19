#ifndef BMEPRESSURE_H
#define BMEPRESSURE_H

#include <Measurements/Pressure/Pressure.h>

#include <Adafruit_BME280.h>

class PressureBME : public Pressure
{
public:

    PressureBME(const char* name,
                Adafruit_BME280& bme);

    void update() override;

private:

    Adafruit_BME280& bme;
};

#endif