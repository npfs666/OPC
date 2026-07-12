#ifndef TEMPERATUREBME_h
#define TEMPERATUREBME_h

#include <Measurements/Temperature/Temperature.h>
#include <Adafruit_BME280.h>

class TemperatureBME : public Temperature
{
public:
    TemperatureBME(const char* name,
                   Adafruit_BME280& bme);

    void update() override;

private:
    Adafruit_BME280& m_bme;
};

#endif