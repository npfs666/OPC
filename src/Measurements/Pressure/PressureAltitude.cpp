// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Pressure/PressureAltitude.h>

#include <SiteSettings.h>

#include <cmath>

namespace
{
    // Atmosphère normalisée (OACI) : pression au niveau de la mer,
    // gradient / température au niveau de la mer, exposant g·M / (R·L).
    constexpr double_t SEA_LEVEL_PRESSURE = 101325.0;
    constexpr double_t LAPSE_RATE_RATIO = 0.0065 / 288.15;
    constexpr double_t EXPONENT = 5.25588;
}

void PressureAltitude::begin(
    const char* name,
    const SiteSettings& site)
{
    Pressure::begin(name);
    this->site = &site;
}

void PressureAltitude::update()
{
    if (site == nullptr)
    {
        setValid(false);
        return;
    }

    setValue(standardPressure(site->altitude));
    setValid(true);
}

uint8_t PressureAltitude::printDecimals() const
{
    return 0;
}

double_t PressureAltitude::standardPressure(double_t altitude)
{
    return SEA_LEVEL_PRESSURE *
           pow(1.0 - LAPSE_RATE_RATIO * altitude, EXPONENT);
}
