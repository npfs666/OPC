// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Humidity/HumidityBME.h>

#include <Hardware/CheckedBME280.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_HUMIDITY = 0.0;
    constexpr double_t MAX_HUMIDITY = 100.0;
}

HumidityBME::HumidityBME()
{
}

void HumidityBME::begin(
    const char* name,
    CheckedBME280& bme)
{
    Humidity::begin(name);
    this->bme = &bme;
}

void HumidityBME::update()
{
    // La bibliothèque ignore les échecs I²C : la puce doit répondre avant et
    // après la lecture (voir CheckedBME280).
    if (bme == nullptr || !bme->responds())
    {
        setValid(false);
        return;
    }

    const double_t value =
        bme->readHumidity();

    if (!bme->responds() ||
        !std::isfinite(value) ||
        value < MIN_HUMIDITY ||
        value > MAX_HUMIDITY)
    {
        setValid(false);
        return;
    }

    setValue(value);
    setValid(true);
}
