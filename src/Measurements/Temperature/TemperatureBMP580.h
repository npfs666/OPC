// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEMPERATUREBMP580_H
#define TEMPERATUREBMP580_H

#include <Measurements/Temperature/Temperature.h>

class Adafruit_BMP5xx;

/**
 * Température interne du BMP581 de la carte mesure : celle du PCB autour du
 * capteur, pas celle de l'air ambiant.
 */
class TemperatureBMP580 : public Temperature
{
public:
    TemperatureBMP580();

    void begin(
        const char* name,
        Adafruit_BMP5xx& bmp580);

    void update() override;

private:
    Adafruit_BMP5xx* bmp580 = nullptr;
};

#endif
