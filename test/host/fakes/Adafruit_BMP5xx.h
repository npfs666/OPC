// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEST_FAKE_ADAFRUIT_BMP5XX_H
#define TEST_FAKE_ADAFRUIT_BMP5XX_H

#include <cmath>

class Adafruit_BMP5xx
{
public:
    // Comme la bibliothèque : en échec, les valeurs précédentes restent.
    bool performReading()
    {
        if (!readingSucceeds)
            return false;

        temperature = nextTemperature;
        pressure = nextPressure;
        return true;
    }

    bool readingSucceeds = true;
    float nextTemperature = NAN;
    float nextPressure = NAN;

    float temperature = NAN;
    float pressure = NAN; // hPa
};

#endif
