// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Temperature/TemperatureBMP580.h>

#include <Adafruit_BMP5xx.h>

#include <cmath>

namespace
{
    // Plage de fonctionnement du BMP581.
    constexpr double_t MIN_TEMPERATURE = -40.0;
    constexpr double_t MAX_TEMPERATURE = 85.0;
}

TemperatureBMP580::TemperatureBMP580()
{
}

void TemperatureBMP580::begin(
    const char* name,
    Adafruit_BMP5xx& bmp580)
{
    Temperature::begin(name);
    this->bmp580 = &bmp580;
    setValue(NAN);
    setStatus(MeasurementStatus::NotReady);
}

void TemperatureBMP580::update()
{
    setValue(NAN);

    // Comme pour la pression : seul performReading() dit si la lecture a
    // abouti, temperature garde sinon la dernière valeur.
    if (bmp580 == nullptr || !bmp580->performReading())
    {
        setStatus(MeasurementStatus::Invalid);
        return;
    }

    const double_t value = bmp580->temperature;

    if (!std::isfinite(value))
    {
        setStatus(MeasurementStatus::Invalid);
        return;
    }

    if (value < MIN_TEMPERATURE)
    {
        setStatus(MeasurementStatus::UnderRange);
        return;
    }

    if (value > MAX_TEMPERATURE)
    {
        setStatus(MeasurementStatus::OverRange);
        return;
    }

    setValue(value);
    setStatus(MeasurementStatus::Ok);
}
