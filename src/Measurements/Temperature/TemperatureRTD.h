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

private:
    Resistance* resistance = nullptr;
};

#endif
