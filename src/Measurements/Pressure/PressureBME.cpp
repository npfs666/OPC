// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Pressure/PressureBME.h>

#include <Hardware/CheckedBME280.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_TEMPERATURE = -40.0;
    constexpr double_t MAX_TEMPERATURE = 85.0;

    constexpr double_t MIN_PRESSURE = 30000.0;
    constexpr double_t MAX_PRESSURE = 110000.0;
}

PressureBME::PressureBME()
{
}

void PressureBME::begin(
    const char* name,
    CheckedBME280& bme)
{
    Pressure::begin(name);
    this->bme = &bme;
}

double_t PressureBME::pressureSeaLevel(int16_t altitude) {

    if (bme == nullptr || !bme->responds())
        return NAN;

    const double_t pressure =
        bme->readPressure();

    const double_t temperature =
        bme->readTemperature();

    if (!bme->responds() ||
        !std::isfinite(pressure) ||
        pressure < MIN_PRESSURE ||
        pressure > MAX_PRESSURE ||
        !std::isfinite(temperature) ||
        temperature < MIN_TEMPERATURE ||
        temperature > MAX_TEMPERATURE)
    {
        return NAN;
    }

    return
        pressure /
        pow(
            1.0 -
                ((0.0065 * altitude) /
                 (temperature + 273.15)),
            5.255);
}

double_t PressureBME::printValue() const {
    return getValue();
}

uint8_t PressureBME::printDecimals() const {
    return 1;
}

void PressureBME::update()
{
    // La bibliothèque ignore les échecs I²C : la puce doit répondre avant et
    // après la lecture (voir CheckedBME280).
    if (bme == nullptr || !bme->responds())
    {
        setValid(false);
        return;
    }

    const double_t value =
        bme->readPressure();

    if (!bme->responds() ||
        !std::isfinite(value) ||
        value < MIN_PRESSURE ||
        value > MAX_PRESSURE)
    {
        setValid(false);
        return;
    }

    setValue(value);
    setValid(true);
}
