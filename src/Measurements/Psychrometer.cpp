#include <Measurements/Psychrometer.h>
#include <math.h>

Psychrometer::Psychrometer(const Temperature& dryBulb,
                           const Temperature& wetBulb,
                           const Pressure& pressure)
    : m_dryBulb(dryBulb),
      m_wetBulb(wetBulb),
      m_pressure(pressure)
{
}

/**
 * Calcule l'humidité relative à partir d'une température sèche et humide
 * 
 * @param m_dryBulb température de la sonde sèche en °C
 * @param m_wetBulb température du bulbe humide en °C
 * @param m_pressure en Pa
 * 
 * @return double_t Humidité relative en %
 */
double_t Psychrometer::relativeHumidity() const
{
    double_t dryTemperature = m_dryBulb.value();
    double_t wetTemperature = m_wetBulb.value();
    double_t atmPressure = m_pressure.value();

    // 1: Calcul de la "constante" psychrométrique
    // Capacité thermique massique de l'air [kJ/kg.°C]
    double_t Cp = (3 * dryTemperature)/50000.0 + 1.005;
    // Energie de vaporiation de l'eau [kJ/kg]
    double_t lambda = -2.3664 * wetTemperature + 2501;
    double_t A = Cp / (lambda * 0.622 ); // [1/°C]
    
    // Atmospheric pressure [P = kPa] [atmPressure = Pa]
    double_t P;
    if ( (atmPressure == NAN) || (atmPressure == 0) )
        P = 101.3;
	else
        P = atmPressure / 1000.0F;

    double_t pVs = 0.6108 * exp((17.27 * wetTemperature)/(wetTemperature + 237.3)); // [kPa]
    double_t pV = pVs - A*P*(dryTemperature-wetTemperature); // [kPa]
    double_t pVs2 = 0.6108 * exp((17.27 * dryTemperature)/(dryTemperature + 237.3)); // [kPa]

    double_t rh = ((double_t)pV/pVs2)*100.0;

    return constrain(rh, 0, 100);
}

double_t Psychrometer::absoluteHumidity() const
{
    return NAN;
}

double_t Psychrometer::dewPoint() const
{
    return NAN;
}