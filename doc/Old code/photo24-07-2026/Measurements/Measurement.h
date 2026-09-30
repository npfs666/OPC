#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <Arduino.h>
#include <hmi/Displayable.h>

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
class Measurement : public Displayable
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

    double_t getValue() const;

    const char *getUnit() const;

    bool isValid() const;

    virtual double_t printValue() const override;


protected:
    void setValue(double_t value);

    void setValid(bool valid = true);

private:
    const char *unit;

    double_t value = 0.0;

    bool valid = false;
};

#endif