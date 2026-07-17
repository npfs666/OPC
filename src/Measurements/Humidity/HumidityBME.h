#ifndef HUMIDITYBME_H
#define HUMIDITYBME_H

#include <Adafruit_BME280.h>

#include <Measurements/Humidity/Humidity.h>

class HumidityBME : public Humidity
{
public:

    HumidityBME(const char* name,
                Adafruit_BME280& bme);

    void update() override;

private:

    Adafruit_BME280& m_bme;
};

#endif