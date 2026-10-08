// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <hmi/Displayable.h>
#include <Configurable.h>
#include <Measurements/MeasurementStatus.h>

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
class Measurement : public Displayable, public Configurable
{
public:
    /**
     * @brief Construit une mesure non initialisée.
     */
    Measurement();

    virtual ~Measurement() = default;

    /**
     * @brief Met à jour la mesure.
     *
     * Entrée : aucune.
     * Sortie : met à jour la valeur et sa validité.
     *
     * Les mesures calculées redéfinissent cette fonction.
     * Les mesures provenant directement d'un capteur peuvent laisser
     * l'implémentation vide.
     */
    virtual void update() = 0;

    /**
     * @brief Filtre la valeur, appelée par ProcessControl juste après
     * update(). Sans effet par défaut.
     *
     * @param[in] now Date de la mesure en ms (uint32_t).
     */
    virtual void applyFilter(uint32_t now)
    {
        (void)now;
    }

    /**
     * @return Dernière valeur mesurée (double_t).
     */
    double_t getValue() const;

    /**
     * @return Unité de la mesure (const char*).
     */
    const char* getUnit() const;

    /**
     * @return true si la mesure est valide (bool).
     */
    bool isValid() const;

    /**
     * @return État de la mesure : Ok, ou la cause de l'invalidité.
     */
    MeasurementStatus getStatus() const;

    /**
     * @brief Affiche la valeur, ou le libellé de l'état si elle est invalide.
     */
    void print(Stream& stream) const override;

    /**
     * @return Valeur destinée à l'affichage (double_t).
     */
    double_t printValue() const override;

    /**
     * @param[in] list Liste recevant les paramètres (ParameterList).
     */
    void registerParameters(
        ParameterList& list) override
    {
        (void)list;
    }

protected:
    /**
     * @brief Initialise une mesure.
     *
     * @param[in] name Nom et clé de la mesure (const char*).
     * @param[in] unit Unité affichée (const char*).
     */
    void begin(
        const char* name,
        const char* unit);

    /**
     * @brief Initialise une mesure avec une clé distincte.
     *
     * @param[in] key Clé de configuration (const char*).
     * @param[in] name Nom affiché (const char*).
     * @param[in] unit Unité affichée (const char*).
     */
    void begin(
        const char* key,
        const char* name,
        const char* unit);

    /**
     * @param[in] value Nouvelle valeur mesurée (double_t).
     */
    void setValue(double_t value);

    /**
     * @param[in] valid true : Ok ; false : Invalid (cause non précisée).
     */
    void setValid(bool valid = true);

    /**
     * @param[in] status Nouvel état de la mesure (MeasurementStatus).
     */
    void setStatus(MeasurementStatus status);

private:
    // Unité associée à la valeur (const char*).
    const char* unit = "";

    // Dernière valeur mesurée (double_t).
    double_t value = 0.0;

    // État de la valeur ; seul Ok la rend exploitable.
    MeasurementStatus status = MeasurementStatus::NotReady;
};

#endif
