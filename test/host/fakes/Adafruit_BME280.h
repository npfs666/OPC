// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEST_FAKE_ADAFRUIT_BME280_H
#define TEST_FAKE_ADAFRUIT_BME280_H

#include <cmath>

class Adafruit_BME280
{
public:
    float readTemperature()
    {
        return temperature;
    }

    float readHumidity()
    {
        return humidity;
    }

    float readPressure()
    {
        return pressure;
    }

    float temperature = NAN;
    float humidity = NAN;
    float pressure = NAN;
};

#endif
