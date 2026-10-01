#ifndef PSYCHROMETER_H
#define PSYCHROMETER_H

#include <cmath>

#include <Measurements/MeasurementStatus.h>

class Pressure;
class Temperature;

class Psychrometer
{
public:

    Psychrometer();

    void begin(const Temperature& dryBulb,
                 const Temperature& wetBulb,
                 const Pressure& pressure);

    bool isValid() const;

    /**
     * Ok si les trois entrées sont exploitables ; sinon l'état de la
     * première entrée en défaut (sèche, humide, pression), ou Invalid.
     */
    MeasurementStatus status() const;

    double_t relativeHumidity() const;

    double_t absoluteHumidity() const;

    double_t dewPoint() const;

private:

    const Temperature* dryBulb = nullptr;
    const Temperature* wetBulb = nullptr;
    const Pressure* pressure = nullptr;
};

#endif
