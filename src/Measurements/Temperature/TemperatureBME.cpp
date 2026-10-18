// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Temperature/TemperatureBME.h>

#include <Hardware/CheckedBME280.h>

#include <cmath>

namespace
{
    constexpr double_t MIN_TEMPERATURE = -40.0;
    constexpr double_t MAX_TEMPERATURE = 85.0;
}

TemperatureBME::TemperatureBME()
{
}


void TemperatureBME::begin(
    const char* name,
    CheckedBME280& bme)
{
    Temperature::begin(name);
    this->bme = &bme;
}

void TemperatureBME::update()
{
    // La bibliothèque ignore les échecs I²C : la puce doit répondre avant et
    // après la lecture (voir CheckedBME280).
    if (bme == nullptr || !bme->responds())
    {
        setValid(false);
        return;
    }

    const double_t value =
        bme->readTemperature();

    if (!bme->responds() ||
        !std::isfinite(value) ||
        value < MIN_TEMPERATURE ||
        value > MAX_TEMPERATURE)
    {
        setValid(false);
        return;
    }

    setValue(value);
    setValid(true);
}
