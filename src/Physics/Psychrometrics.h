// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PHYSICS_PSYCHROMETRICS_H
#define PHYSICS_PSYCHROMETRICS_H

#include <cmath>

namespace Physics
{
    /**
     * Psychrométrie, formules WMO (guide n° 8, annexe 4.B) :
     * - pression de vapeur saturante de Magnus, coefficients de Sonntag
     *   (1990), sur l'eau et sur la glace ;
     * - équation du psychromètre e = e'(Tw) - A·P·(T - Tw).
     *
     * L'humidité relative est toujours rapportée à l'eau liquide, y compris
     * sous 0 °C (convention WMO).
     */
    class Psychrometrics final
    {
    public:
        /**
         * Coefficient d'un psychromètre ventilé de type Assmann, bulbe
         * mouillé (1/°C). Sur la glace : ICE_COEFFICIENT_RATIO fois moins.
         */
        static constexpr double_t ASSMANN_COEFFICIENT = 6.53e-4;

        /**
         * Rapport A glace / A eau du psychromètre (WMO : 5,75 / 6,53).
         */
        static constexpr double_t ICE_COEFFICIENT_RATIO = 5.75 / 6.53;

        /**
         * @brief Pression de vapeur saturante sur l'eau liquide (Pa)
         *
         * @param temperature Température (°C), -45 à 60 °C selon la WMO
         */
        static double_t saturationVaporPressure(
            double_t temperature);

        /**
         * @brief Pression de vapeur saturante sur la glace (Pa)
         *
         * @param temperature Température (°C), -65 à 0,01 °C selon la WMO
         */
        static double_t saturationVaporPressureIce(
            double_t temperature);

        /**
         * @brief Pression de vapeur réelle par l'équation du psychromètre (Pa)
         *
         * @param dryBulb Température sèche (°C)
         * @param wetBulb Température humide (°C)
         * @param pressure Pression atmosphérique (Pa)
         * @param coefficient Coefficient A du bulbe mouillé (1/°C)
         * @param iceBulb true : bulbe givré (formules sur la glace)
         */
        static double_t vaporPressure(
            double_t dryBulb,
            double_t wetBulb,
            double_t pressure,
            double_t coefficient,
            bool iceBulb);

        /**
         * @brief Humidité relative (%) par méthode psychrométrique
         *
         * Valeur brute, non bornée : négative si le bulbe humide est trop
         * froid, au-delà de 100 % s'il est plus chaud que le sec.
         *
         * @param dryBulb Température sèche (°C)
         * @param wetBulb Température humide (°C)
         * @param pressure Pression atmosphérique (Pa)
         * @param coefficient Coefficient A du bulbe mouillé (1/°C)
         * @param iceBulb true : bulbe givré (formules sur la glace)
         * @return Humidité relative (%)
         */
        static double_t relativeHumidity(
            double_t dryBulb,
            double_t wetBulb,
            double_t pressure,
            double_t coefficient = ASSMANN_COEFFICIENT,
            bool iceBulb = false);

        /**
         * @brief Point de rosée (°C), inverse exact de
         * saturationVaporPressure() : dewPoint(T, 100) == T.
         *
         * @return NAN si l'humidité n'est pas strictement positive
         */
        static double_t dewPoint(
            double_t temperature,
            double_t humidity);

        /**
         * @brief Humidité absolue (g/m³)
         */
        static double_t absoluteHumidity(
            double_t temperature,
            double_t humidity);

    private:
        static constexpr double_t kelvin = 273.15;

        // Magnus, Sonntag 1990 : 611,2 Pa · exp(a·t / (b + t))
        static constexpr double_t magnusPressure = 611.2;
        static constexpr double_t waterA = 17.62;
        static constexpr double_t waterB = 243.12;
        static constexpr double_t iceA = 22.46;
        static constexpr double_t iceB = 272.62;
    };
}

#endif
