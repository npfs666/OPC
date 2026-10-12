// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Physics/Psychrometrics.h>
#include <math.h>

double_t Physics::Psychrometrics::saturationVaporPressure(
    double_t temperature)
{
    return magnusPressure * exp(
               (waterA * temperature) /
               (waterB + temperature));
}

double_t Physics::Psychrometrics::saturationVaporPressureIce(
    double_t temperature)
{
    return magnusPressure * exp(
               (iceA * temperature) /
               (iceB + temperature));
}

double_t Physics::Psychrometrics::vaporPressure(
    double_t dryBulb,
    double_t wetBulb,
    double_t pressure,
    double_t coefficient,
    bool iceBulb)
{
    if (iceBulb)
    {
        return saturationVaporPressureIce(wetBulb) -
               coefficient * ICE_COEFFICIENT_RATIO *
                   pressure * (dryBulb - wetBulb);
    }

    // Dépendance en température du coefficient (WMO).
    return saturationVaporPressure(wetBulb) -
           coefficient * (1.0 + 0.000944 * wetBulb) *
               pressure * (dryBulb - wetBulb);
}

double_t Physics::Psychrometrics::relativeHumidity(
    double_t dryBulb,
    double_t wetBulb,
    double_t pressure,
    double_t coefficient,
    bool iceBulb)
{
    const double_t e = vaporPressure(
        dryBulb,
        wetBulb,
        pressure,
        coefficient,
        iceBulb);

    return e / saturationVaporPressure(dryBulb) * 100.0;
}

double_t Physics::Psychrometrics::dewPoint(
    double_t temperature,
    double_t humidity)
{
    if (!(humidity > 0.0))
        return NAN;

    const double_t alpha =
        ((waterA * temperature) /
         (waterB + temperature)) +
        log(humidity / 100.0);

    return (waterB * alpha) /
           (waterA - alpha);
}

double_t Physics::Psychrometrics::absoluteHumidity(
    double_t temperature,
    double_t humidity)
{
    const double_t es = saturationVaporPressure(temperature);

    const double_t e = humidity / 100.0 * es;

    // Gaz parfait : 1000 / Rv, Rv = 461,5 J/(kg·K)
    return (2.16679 * e) / (temperature + kelvin);
}
