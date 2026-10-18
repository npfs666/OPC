// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEST_FAKE_ADAFRUIT_BME280_H
#define TEST_FAKE_ADAFRUIT_BME280_H

#include <cmath>
#include <cstdint>

enum
{
    BME280_REGISTER_CHIPID = 0xD0
};

class Adafruit_BME280
{
public:
    // Identifiant lu par begin() sur un BME280.
    uint32_t sensorID()
    {
        return 0x60;
    }

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

    // Capteur muet : comme la bibliothèque, une lecture ratée laisse
    // l'adresse du registre dans le tampon, et les mesures rendent des
    // valeurs plausibles.
    bool responding = true;

protected:
    uint8_t read8(uint8_t reg)
    {
        return responding ? 0x60 : reg;
    }
};

#endif
