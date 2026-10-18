// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEMPERATUREBME_h
#define TEMPERATUREBME_h

#include <Measurements/Temperature/Temperature.h>

class CheckedBME280;

class TemperatureBME : public Temperature
{
public:
    TemperatureBME();

    void begin(
        const char* name,
        CheckedBME280& bme);

    void update() override;

private:
    CheckedBME280* bme = nullptr;
};

#endif
