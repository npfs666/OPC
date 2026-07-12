#ifndef PSYCHROMETER_H
#define PSYCHROMETER_H

#include <Arduino.h>

class Psychrometer
{
public:
    /**
     * @brief Humidité relative (%) par méthode psychrométrique
     *
     * @param dryBulb Température sèche (°C)
     * @param wetBulb Température humide (°C)
     * @param pressure Pression atmosphérique (Pa)
     * @return Humidité relative (%)
     */
    static double_t relativeHumidity(double_t dryBulb, double_t wetBulb, double_t pressure);

    /**
     * @brief Pression de vapeur saturante (Pa)
     */
    static double_t saturationVaporPressure(double_t temperature);

    /**
     * @brief Pression de vapeur réelle (Pa)
     */
    static double_t vaporPressure(double_t dryBulb, double_t wetBulb, double_t pressure);

    /**
     * @brief Point de rosée (°C)
     */
    static double_t dewPoint(double_t temperature, double_t humidity);

    /**
     * @brief Humidité absolue (g/m³)
     */
    static double_t absoluteHumidity(double_t temperature, double_t humidity);

    /**
     * @brief Humidité relative (%) par méthode psychrométrique, mon calcul, pas celui de l'IA
     *
     * @param dryBulb Température sèche (°C)
     * @param wetBulb Température humide (°C)
     * @param pressure Pression atmosphérique (Pa)
     *
     * @return Humidité relative (%)
     */
    static double_t getRH(double_t dryBulb, double_t wetBulb, double_t pressure);

private:
    static constexpr double_t kelvin = 273.15;
};

#endif