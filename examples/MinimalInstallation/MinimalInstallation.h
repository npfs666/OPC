#ifndef MINIMAL_INSTALLATION_H
#define MINIMAL_INSTALLATION_H

#include <Installation.h>

#include <Hardware/RTDSensor.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

class Adafruit_BME280;
class ProcessControl;
class SensorBoard;

class MinimalInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BME280& bme,
        ProcessControl& process) override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    RTDSensor temperatureInput;
    Resistance temperatureResistance;
    TemperatureRTD temperature;
};

#endif
