#ifndef TEMPERATUREBME_h
#define TEMPERATUREBME_h

#include <Measurements/Temperature/Temperature.h>

class Adafruit_BME280;

class TemperatureBME : public Temperature
{
public:
    TemperatureBME();

    void begin(const char* name,Adafruit_BME280& bme);

    void update() override;

private:
    Adafruit_BME280* bme;
};

#endif
