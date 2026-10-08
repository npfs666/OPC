// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PT100_H
#define PT100_H

#include <cmath>
#include <cstdint>

class PT100
{
public:

    /**
     * @brief Convertit une résistance PT100 en température (°C)
     *
     * @param resistance Résistance en Ohms
     * @return Température en °C
     */
    static double_t getResistanceToTemperature(double_t Rrtd);

    /**
     * @brief Convertit une résistance PT100 en température (°C) en
     *        inversant l'équation de Callendar-Van Dusen (IEC 60751)
     *        par la méthode de Newton-Raphson
     *
     * @param resistance Résistance en Ohms
     * @return Température en °C, NAN si hors plage (-200 °C à 850 °C)
     *         ou si la méthode ne converge pas
     */
    static double_t getResistanceToTemperatureNewton(double_t resistance);

    /**
     * @brief Résistance d'une PT100 à une température donnée
     *        (Callendar-Van Dusen, IEC 60751)
     *
     * @param temperature Température en °C
     * @return Résistance en ohms (pas de contrôle de plage)
     */
    static double_t getTemperatureToResistance(double_t temperature);

    // Étendue de la norme IEC 60751.
    static constexpr double_t MINIMUM_TEMPERATURE = -200.0;
    static constexpr double_t MAXIMUM_TEMPERATURE = 850.0;

private:

    /*
     * Coefficients Callendar-Van Dusen, IEC 60751 (alpha = 0.00385)
     */
    static constexpr double_t R0 = 100.0;
    static constexpr double_t A = 3.9083e-3;
    static constexpr double_t B = -5.775e-7;
    static constexpr double_t C = -4.183e-12;

    static constexpr uint16_t interpolationSize = 21;

    static const float interpolationTable[interpolationSize];
};

#endif
