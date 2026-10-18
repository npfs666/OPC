// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BMEPRESSURE_H
#define BMEPRESSURE_H

#include <Measurements/Pressure/Pressure.h>

class CheckedBME280;

class PressureBME : public Pressure
{
public:

    PressureBME();

    void begin(
        const char* name,
        CheckedBME280& bme);

    double_t pressureSeaLevel(int16_t altitude);

    void update() override;

    double_t printValue() const override;

    uint8_t printDecimals() const override;

private:

    CheckedBME280* bme = nullptr;
};

#endif
