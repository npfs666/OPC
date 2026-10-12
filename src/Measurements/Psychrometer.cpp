// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Psychrometer.h>

#include <Measurements/Pressure/Pressure.h>
#include <Measurements/Temperature/Temperature.h>
#include <Physics/Psychrometrics.h>

#include <algorithm>
#include <cmath>

Psychrometer::Psychrometer()
{
}

void Psychrometer::begin(const Temperature& dryBulb,
                           const Temperature& wetBulb,
                           const Pressure& pressure)
{
    this->dryBulb = &dryBulb;
    this->wetBulb = &wetBulb;
    this->pressure = &pressure;
}

Psychrometer::Settings& Psychrometer::settings()
{
    return configuration;
}

const Psychrometer::Settings& Psychrometer::settings() const
{
    return configuration;
}

Psychrometer::Result Psychrometer::evaluate() const
{
    Result result;

    if (dryBulb == nullptr ||
        wetBulb == nullptr ||
        pressure == nullptr)
    {
        return result;
    }

    if (!dryBulb->isValid())
    {
        result.status = dryBulb->getStatus();
        return result;
    }

    if (!wetBulb->isValid())
    {
        result.status = wetBulb->getStatus();
        return result;
    }

    if (!pressure->isValid())
    {
        result.status = pressure->getStatus();
        return result;
    }

    const double_t dryTemperature =
        dryBulb->getValue();

    const double_t wetTemperature =
        wetBulb->getValue();

    const double_t atmosphericPressure =
        pressure->getValue();

    if (!std::isfinite(dryTemperature) ||
        !std::isfinite(wetTemperature) ||
        !std::isfinite(atmosphericPressure) ||
        atmosphericPressure <= 0.0)
    {
        return result;
    }

    // Un bulbe humide ne peut pas être plus chaud que le sec.
    if (wetTemperature > dryTemperature + WET_ABOVE_DRY_TOLERANCE)
        return result;

    const bool iceBulb =
        wetTemperature < 0.0 &&
        configuration.frozenBulb == FrozenBulb::Ice;

    const double_t humidity =
        Physics::Psychrometrics::relativeHumidity(
            dryTemperature,
            wetTemperature,
            atmosphericPressure,
            configuration.coefficient * 1e-4,
            iceBulb);

    if (!std::isfinite(humidity))
        return result;

    if (humidity < -NEGATIVE_HUMIDITY_TOLERANCE)
    {
        result.status = MeasurementStatus::UnderRange;
        return result;
    }

    result.status = MeasurementStatus::Ok;
    result.relativeHumidity = std::min(std::max(humidity, 0.0), 100.0);
    return result;
}

bool Psychrometer::isValid() const
{
    return status() == MeasurementStatus::Ok;
}

MeasurementStatus Psychrometer::status() const
{
    return evaluate().status;
}

/**
 * Humidité relative (%) à partir des températures sèche et humide et de la
 * pression ; NAN si le calcul n'est pas Ok (voir evaluate()).
 */
double_t Psychrometer::relativeHumidity() const
{
    return evaluate().relativeHumidity;
}

double_t Psychrometer::absoluteHumidity() const
{
    const double_t humidity =
        relativeHumidity();

    if (!std::isfinite(humidity))
        return NAN;

    return Physics::Psychrometrics::absoluteHumidity(
        dryBulb->getValue(),
        humidity);
}

double_t Psychrometer::dewPoint() const
{
    const double_t humidity =
        relativeHumidity();

    if (!std::isfinite(humidity))
        return NAN;

    return Physics::Psychrometrics::dewPoint(
        dryBulb->getValue(),
        humidity);
}
