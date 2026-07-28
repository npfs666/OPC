#include "PT100.h"
#include <cmath>

/* 
    Array of fixed values RTD interpolation
    each line is +10 Ohms, starting from 0 (0 Ohms <-> -500°C, 10 <-> -219.415, ...)
*/
const float PT100::interpolationTable[interpolationSize] =
    {-500.00, -219.415, -196.509, -173.118, -149.304, -125.122, -100.617, -75.827, -50.781, -25.501,
     0.000, 25.686, 51.571, 77.660, 103.958, 130.469, 157.198, 184.152, 211.336, 238.756,
     266.419};


/**
 * @brief Conversion d'une resistance en température via le calcul par interpolation (plus précis qu'une fonction pour une approche réelle)
 * 
 * @param resistance Résistance de la PT100 en ohms
 * @return double_t temperature in °C
 */
double_t PT100::getResistanceToTemperature(
    double_t resistance)
{
    constexpr double_t resistanceStep = 10.0;

    const double_t minimumResistance =
        resistanceStep;

    const double_t maximumResistance =
        (interpolationSize - 1) *
        resistanceStep;

    if (!std::isfinite(resistance) ||
        resistance < minimumResistance ||
        resistance > maximumResistance)
    {
        return NAN;
    }

    const double_t tablePosition =
        resistance / resistanceStep;

    const uint16_t index =
        static_cast<uint16_t>(
            tablePosition);

    const double_t fraction =
        tablePosition - index;

    /*
     * Une valeur exactement présente dans la table ne doit
     * pas être interpolée. Ce cas protège également la
     * dernière entrée contre un accès à index + 1.
     */
    if (fraction == 0.0 ||
        index >= interpolationSize - 1)
    {
        return interpolationTable[index];
    }

    /*
     * La première plage utilisable n'a pas de point
     * précédent exploitable : interpolation linéaire entre
     * les deux premières températures valides.
     */
    if (index == 1)
    {
        const double_t lower =
            interpolationTable[index];

        const double_t upper =
            interpolationTable[index + 1];

        return lower +
               fraction * (upper - lower);
    }

    /*
     * Interpolation quadratique à trois points pour toutes
     * les autres plages.
     */
    const double_t previous =
        interpolationTable[index - 1];

    const double_t current =
        interpolationTable[index];

    const double_t next =
        interpolationTable[index + 1];

    return current +
           0.5 * fraction *
               (next - previous +
                fraction *
                    (previous -
                     2.0 * current +
                     next));
}
