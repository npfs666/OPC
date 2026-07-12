#include "Psychrometer.h"
#include <math.h>

double_t Psychrometer::relativeHumidity(double_t dryBulb, double_t wetBulb, double_t pressure)
{
    const double_t es = saturationVaporPressure(dryBulb);

    const double_t e = vaporPressure(dryBulb, wetBulb, pressure);

    return (e / es) * 100.0;
}

double_t Psychrometer::saturationVaporPressure(double_t temperature)
{
    return 610.78 * exp(
               (17.2694 * temperature) /
               (temperature + 238.3));
}

double_t Psychrometer::vaporPressure(double_t dryBulb, double_t wetBulb, double_t pressure)
{
    const double_t ew = saturationVaporPressure(wetBulb);

    const double_t gamma =
        0.00066 *
        (1.0 + 0.00115 * wetBulb) *
        pressure;

    return ew - gamma * (dryBulb - wetBulb);
}

double_t Psychrometer::dewPoint(double_t temperature, double_t humidity)
{
    const double_t a = 17.27;
    const double_t b = 237.7;

    const double_t alpha =
        ((a * temperature) /
         (b + temperature)) +
        log(humidity / 100.0);

    return (b * alpha) /
           (a - alpha);
}

double_t Psychrometer::absoluteHumidity(double_t temperature, double_t humidity)
{
    const double_t es = saturationVaporPressure(temperature);

    const double_t e = humidity / 100.0 * es;

    return (2.16679 * e) / (temperature + kelvin);
}

double_t Psychrometer::getRH(double_t dryBulb, double_t wetBulb, double_t pressure)
{

    // 1: Calcul de la "constante" psychrométrique
    // Capacité thermique massique de l'air [kJ/kg.°C]
    // 0.00006 * tempSeche + 1.005; ancien calcul
    double_t Cp = (3 * dryBulb) / 50000.0 + 1.005;
    // Energie de vaporiation de l'eau [kJ/kg]
    double_t lambda = -2.3664 * wetBulb + 2501;
    double_t A = Cp / (lambda * 0.622); // [1/°C]

    // Atmospheric pressure [kPa]
    double_t P = pressure / 1000.0F;

    double_t pVs = 0.6108 * pow(2.71828, ((17.27 * wetBulb) / (wetBulb + 237.3)));  // [kPa]
    double_t pV = pVs - A * P * (dryBulb - wetBulb);                                // [kPa]
    double_t pVs2 = 0.6108 * pow(2.71828, ((17.27 * dryBulb) / (dryBulb + 237.3))); // [kPa]

    // Ancien calcul
    // double_t pVs = pow(10,(2.7877+(7.625*mes1)/(241.6+mes1)));
    // double_t pV = pVs - 0.000667*102.3000*(mes2-mes1);
    // double_t pVs2 = pow(10,(2.7877+(7.625*mes2)/(241.6+mes2)));

    double_t rh = ((double_t)pV / pVs2) * 100.0;

    return rh;
}