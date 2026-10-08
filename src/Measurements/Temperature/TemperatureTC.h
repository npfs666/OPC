// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEMPERATURE_TC_H
#define TEMPERATURE_TC_H

#include <Measurements/Temperature/Temperature.h>
#include <Hardware/Sensor.h>

class TemperatureTC : public Temperature
{
public:
    void begin(const char* name, Sensor& sensor);

    void update() override;

    // Filtre réglé sur la sonde (Sensor::Settings::filterTime).
    void applyFilter(uint32_t now) override;

private:
    Sensor* sensor = nullptr;
};

#endif
