// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HUMIDITYBME_H
#define HUMIDITYBME_H

#include <Measurements/Humidity/Humidity.h>

class CheckedBME280;

class HumidityBME : public Humidity
{
public:

    HumidityBME();

    void begin(
        const char* name,
        CheckedBME280& bme);

    void update() override;

private:

    CheckedBME280* bme = nullptr;
};

#endif
