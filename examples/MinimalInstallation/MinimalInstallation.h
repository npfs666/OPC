// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MINIMAL_INSTALLATION_H
#define MINIMAL_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

class MinimalInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    Sensor temperatureInput;
    Resistance temperatureResistance;
    TemperatureRTD temperature;
};

#endif
