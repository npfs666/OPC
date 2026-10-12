// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PSYCHROMETER_H
#define PSYCHROMETER_H

#include <cmath>
#include <cstdint>

#include <Measurements/MeasurementStatus.h>

class Pressure;
class Temperature;

class Psychrometer
{
public:

    /**
     * Bulbe humide sous 0 °C : mèche givrée (formules sur la glace) ou
     * eau surfondue (formules sur l'eau).
     */
    enum class FrozenBulb : uint8_t
    {
        Ice,
        Supercooled
    };

    struct Settings
    {
        // Coefficient A du bulbe mouillé, en 1e-4/°C (6,53 : ventilé).
        double_t coefficient = 6.53;

        FrozenBulb frozenBulb = FrozenBulb::Ice;
    };

    struct Result
    {
        MeasurementStatus status = MeasurementStatus::Invalid;

        // Humidité relative (%), bornée à 0..100 ; NAN hors Ok.
        double_t relativeHumidity = NAN;
    };

    /**
     * Bulbe humide plus chaud que le sec au-delà de cet écart (K) :
     * incohérence (sondes inversées, décalées), état Invalid. En deçà,
     * l'humidité est ramenée à 100 %.
     */
    static constexpr double_t WET_ABOVE_DRY_TOLERANCE = 0.5;

    /**
     * Humidité calculée sous -NEGATIVE_HUMIDITY_TOLERANCE (%) : bulbe
     * humide trop froid pour l'air mesuré, état UnderRange. En deçà,
     * l'humidité est ramenée à 0 %.
     */
    static constexpr double_t NEGATIVE_HUMIDITY_TOLERANCE = 2.0;

    Psychrometer();

    void begin(const Temperature& dryBulb,
                 const Temperature& wetBulb,
                 const Pressure& pressure);

    Settings& settings();

    const Settings& settings() const;

    /**
     * Humidité relative et son état, calculés ensemble.
     *
     * État : celui de la première entrée en défaut (sèche, humide,
     * pression), Invalid si une valeur est inexploitable ou si le bulbe
     * humide est plus chaud que le sec, UnderRange si l'humidité calculée
     * est négative, Ok sinon.
     */
    Result evaluate() const;

    bool isValid() const;

    MeasurementStatus status() const;

    double_t relativeHumidity() const;

    double_t absoluteHumidity() const;

    double_t dewPoint() const;

private:

    const Temperature* dryBulb = nullptr;
    const Temperature* wetBulb = nullptr;
    const Pressure* pressure = nullptr;

    Settings configuration;
};

#endif
