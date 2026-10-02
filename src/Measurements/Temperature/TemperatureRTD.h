#ifndef RTDTEMPERATURE_h
#define RTDTEMPERATURE_h

#include <Measurements/Temperature/Temperature.h>

class Resistance;

class TemperatureRTD : public Temperature
{
public:

    TemperatureRTD();

    void begin(const char* name,
                   Resistance& resistance);

    void update() override;

    // Filtre réglé sur la sonde (Sensor::Settings::filterTime).
    void applyFilter(uint32_t now) override;

private:
    Resistance* resistance = nullptr;
};

#endif
