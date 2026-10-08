// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OUTPUT_H
#define OUTPUT_H

#include <hmi/Displayable.h>
#include <Configurable.h>

class ParameterEditor;

/**
 * Compteurs d'entretien d'une sortie tout-ou-rien, sauvegardés dans
 * /counters.json (voir OPC).
 */
struct OutputCounters
{
    uint32_t switches = 0;          // basculements réels de la sortie
    double_t onSeconds = 0.0;       // durée cumulée en marche

    // Seuil d'entretien en manœuvres (réglage), 0 = aucun.
    uint32_t maintenanceLimit = 0;

    bool maintenanceDue() const
    {
        return maintenanceLimit > 0 && switches >= maintenanceLimit;
    }
};

class Output : public Displayable, public Configurable
{
public:
    Output();

    void begin(const char* name);

    void begin(
        const char* key,
        const char* name);

    virtual ~Output() = default;

    virtual bool begin() = 0;

    void setCommand(
        double_t value,
        uint32_t now);

    virtual void poll(uint32_t now) = 0;

    virtual void forceSafe() = 0;

    /** Commande appliquée par forceSafe(), de 0 à 1. */
    virtual double_t safeCommand() const = 0;

    /**
     * Impose la commande de sécurité et la rend non modifiable dans le menu
     * (un relais la ramène à ON ou OFF, voir RelayOutput::lockSafeState()).
     */
    virtual void lockSafeCommand(double_t safeCommand) = 0;

    virtual bool applySettings() = 0;

    virtual bool isHealthy() const = 0;

    double_t requestedCommand() const;
    double_t appliedCommand() const;
    uint32_t lastCommandAt() const;

    /**
     * Vrai si la sortie retarde la commande demandée (temps minimal de
     * marche ou d'arrêt d'un relais).
     */
    virtual bool isWaiting() const
    {
        return false;
    }

    /** Compteurs d'entretien, ou nullptr si la sortie n'en a pas (PWM). */
    virtual const OutputCounters* counters() const
    {
        return nullptr;
    }

    /** Durée en marche, manœuvre en cours comprise, en secondes. */
    virtual double_t onSeconds() const
    {
        return 0.0;
    }

    /** Clé de configuration stable, aussi utilisée dans /counters.json. */
    const char* configurationKey() const
    {
        return getConfigurationKey();
    }

    /** Clé du sous-menu de ses compteurs, ou nullptr. */
    virtual const char* countersOwnerKey() const
    {
        return nullptr;
    }

    /** Remet les compteurs à zéro (après l'entretien). */
    virtual void resetCounters()
    {
    }

    /** Recharge les compteurs sauvegardés. */
    virtual void restoreCounters(
        uint32_t switches,
        double_t onSeconds)
    {
        (void)switches;
        (void)onSeconds;
    }

    void registerParameters(
        ParameterList& list) override
    {
        (void)list;
    }

protected:
    /**
     * Vrai si aucune autre sortie n'utilise la même broche dans les
     * brouillons (paramètres "pin" de la catégorie "outputs").
     */
    static bool pinIsUnique(
        const ParameterEditor& editor,
        const char* ownerKey);

    void setAppliedCommand(double_t value);

    double_t printValue() const override;
    const char* getUnit() const override;

    double_t requested = 0.0;
    double_t applied = 0.0;
    uint32_t lastCommandTime = 0;
};

#endif
