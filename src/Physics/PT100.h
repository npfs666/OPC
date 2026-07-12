#ifndef PT100_H
#define PT100_H

#include <Arduino.h>


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
     * @brief Convertit une température en résistance PT100
     *
     * @param temperature Température en °C
     * @return Résistance en Ohms
     */
    static double_t temperatureToResistance(double_t temperature);

private:

    static constexpr uint16_t interpolationSize = 21;

    static const float interpolationTable[interpolationSize];
};

#endif