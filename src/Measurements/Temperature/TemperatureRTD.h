#ifndef RTDTEMPERATURE_h
#define RTDTEMPERATURE_h

#include <Measurements/Temperature/Temperature.h>
#include <Measurements/Resistance.h>

class TemperatureRTD : public Temperature
{
public:
    TemperatureRTD(const char* name,
                   Resistance& resistance);

    void update() override;

private:
    Resistance& m_resistance;
};

#endif