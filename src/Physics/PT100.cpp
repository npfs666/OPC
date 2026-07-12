#include "PT100.h"
#include <math.h>

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
 * @param id sensor index
 * @return double_t temperature in °C
 */
double_t PT100::getResistanceToTemperature(double_t Rrtd) {

    if(Rrtd <= 0 )
        return -300.0f;

    if(Rrtd >= interpolationTable[interpolationSize - 1])
        return 300.0f;

    int16_t index=(int16_t) (Rrtd/10);
    double_t frac = (double_t)(Rrtd/10.0) - index;
    double_t a = interpolationTable[index];

    double_t temperature = 0;

    // Si valeur juste, on lis la case directement
    if (index == Rrtd / 10)
    {
        temperature = interpolationTable[index];
    }
    // Sinon approximation par interpolation du des valeurs du tableau
    double_t b = interpolationTable[index + 1] / 2.0;
    double_t c = interpolationTable[index - 1] / 2.0;
    temperature = (double_t)a + frac * (b - c + frac * (c + b - a));

    return temperature;
}