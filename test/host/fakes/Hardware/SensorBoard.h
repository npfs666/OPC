#ifndef HOST_FAKE_SENSORBOARD_H
#define HOST_FAKE_SENSORBOARD_H
#include <Hardware/Sensor.h>

// Double de la carte : les tests de mesure contrôlent la tension et l'ADC
// sans simuler SPI. Le pilote réel est vérifié par la compilation embarquée.
class SensorBoard
{
public:
    struct Settings { double coldJunctionOffset = 0.0; } settings;
    double adcTemperature = NAN;
    double voltageMv = NAN;
    bool addSensor(Sensor& sensor)
    {
        if (sensor.board != nullptr) return false;
        sensor.board = this;
        return true;
    }
    double computeVoltage(const Sensor&) const { return voltageMv; }
    double getColdJunctionTemperature() const
    {
        return adcTemperature + settings.coldJunctionOffset;
    }
};
#endif
