// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HOST_FAKE_SENSORBOARD_H
#define HOST_FAKE_SENSORBOARD_H
#include <Hardware/Sensor.h>

#include <cmath>
#include <cstddef>

class ParameterList;

// Double de la carte : les tests contrôlent la résistance, la tension et l'ADC
// sans simuler SPI. Le pilote réel est vérifié par la compilation embarquée.
class SensorBoard
{
public:
    struct Settings { double coldJunctionOffset = 0.0; } settings;
    double adcTemperature = NAN;
    double voltageMv = NAN;
    double resistanceOhms = NAN;
    // Résistance propre à chaque sonde, dans l'ordre d'addSensor() ; NaN :
    // resistanceOhms. Pour tester une installation à plusieurs sondes.
    static constexpr size_t MAX_SENSORS = 4;
    const Sensor* sensors[MAX_SENSORS] = {};
    double sensorResistances[MAX_SENSORS] = {NAN, NAN, NAN, NAN};
    size_t sensorCount = 0;

    bool addSensor(Sensor& sensor)
    {
        if (sensor.board != nullptr) return false;
        sensor.board = this;
        if (sensorCount < MAX_SENSORS) sensors[sensorCount++] = &sensor;
        return true;
    }
    double computeVoltage(const Sensor&) const { return voltageMv; }
    double computeResistance(Sensor& sensor) const
    {
        for (size_t i = 0; i < sensorCount; i++)
        {
            if (sensors[i] == &sensor && !std::isnan(sensorResistances[i]))
                return sensorResistances[i];
        }
        return resistanceOhms;
    }
    void registerParameters(ParameterList&) {}
    double getColdJunctionTemperature() const
    {
        return adcTemperature + settings.coldJunctionOffset;
    }
};
#endif
