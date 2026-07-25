#ifndef HUMIDITYBME_H
#define HUMIDITYBME_H

#include <Adafruit_BME280.h>

#include <Measurements/Humidity/Humidity.h>

class HumidityBME : public Humidity
{
public:

    HumidityBME();

    void begin(const char* name, Adafruit_BME280& bme);

    void update() override;

private:

    Adafruit_BME280* bme = nullptr;
};

#endif