#ifndef TEMPERATURE_TC_H
#define TEMPERATURE_TC_H

#include <Measurements/Temperature/Temperature.h>
#include <Hardware/Sensor.h>

class TemperatureTC : public Temperature
{
public:
    void begin(const char* name, Sensor& sensor);

    void update() override;

private:
    Sensor* sensor = nullptr;
};

#endif
