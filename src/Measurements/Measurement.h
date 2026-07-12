#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <Arduino.h>

/**
 * @brief Classe de base de toutes les grandeurs physiques.
 *
 * Une Measurement représente une valeur physique :
 *  - Résistance
 *  - Température
 *  - Humidité
 *  - Point de rosée
 *  - etc...
 *
 * Elle ne connait pas le matériel.
 */
class Measurement
{
public:

    enum class MeasurementType
    {
        Resistance,
        Temperature,
        Pressure,
        Humidity,
        DewPoint,
        Delta,
        Average
    };

    Measurement(const char *name,
                const char *unit);

    virtual ~Measurement() = default;

    /**
     * @brief Met à jour la mesure.
     *
     * Les mesures calculées redéfinissent cette fonction.
     * Les mesures provenant directement d'un capteur peuvent laisser
     * l'implémentation vide.
     */
    virtual void update() = 0;

    MeasurementType type() const;

    double_t value() const;

    const char *name() const;

    const char *unit() const;

    bool valid() const;

protected:
    void setValue(double_t value);

    void setValid(bool valid = true);

private:
    const char *m_name;

    const char *m_unit;

    double_t m_value = 0.0;

    bool m_valid = false;
};

#endif