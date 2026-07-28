#include <Measurements/Psychrometer.h>

#include <Physics/Psychrometrics.h>

#include <cmath>

Psychrometer::Psychrometer()
{
}

void Psychrometer::begin(const Temperature& dryBulb,
                           const Temperature& wetBulb,
                           const Pressure& pressure)
{
    this->dryBulb = &dryBulb;
    this->wetBulb = &wetBulb;
    this->pressure = &pressure;
}

bool Psychrometer::isValid() const
{
    if (dryBulb == nullptr ||
        wetBulb == nullptr ||
        pressure == nullptr)
    {
        return false;
    }

    if (!dryBulb->isValid() ||
        !wetBulb->isValid() ||
        !pressure->isValid())
    {
        return false;
    }

    const double_t dryTemperature =
        dryBulb->getValue();

    const double_t wetTemperature =
        wetBulb->getValue();

    const double_t atmosphericPressure =
        pressure->getValue();

    return
        std::isfinite(dryTemperature) &&
        std::isfinite(wetTemperature) &&
        std::isfinite(atmosphericPressure) &&
        atmosphericPressure > 0.0;
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
    if (!isValid())
        return NAN;

    const double_t humidity =
        Physics::Psychrometrics::getRH(
            dryBulb->getValue(),
            wetBulb->getValue(),
            pressure->getValue());

    if (!std::isfinite(humidity))
        return NAN;

    return constrain(
        humidity,
        0.0,
        100.0);
}

double_t Psychrometer::absoluteHumidity() const
{
    const double_t humidity =
        relativeHumidity();

    if (!std::isfinite(humidity))
        return NAN;

    return Physics::Psychrometrics::absoluteHumidity(
        dryBulb->getValue(),
        humidity);
}

double_t Psychrometer::dewPoint() const
{
    const double_t humidity =
        relativeHumidity();

    if (!std::isfinite(humidity))
        return NAN;

    return Physics::Psychrometrics::dewPoint(
        dryBulb->getValue(),
        humidity);
}
