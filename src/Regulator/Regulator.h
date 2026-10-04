#ifndef REGULATOR_H
#define REGULATOR_H

#include <hmi/Displayable.h>
#include <Configurable.h>
#include <Measurements/MeasurementStatus.h>

class Alarm;

class Regulator : public Displayable, public Configurable
{
public:
    /**
     * Réaction à un défaut de la mesure d'entrée (rupture, court-circuit,
     * hors étendue) :
     *  - SafeState : commande invalide, sorties en état sûr (défaut) ;
     *  - Hold : la commande d'avant le défaut est maintenue au plus
     *    holdTime secondes, pour passer une coupure brève, puis état sûr.
     * Pas de sortie forcée : sans mesure, l'état sûr reste la règle.
     */
    enum class FaultAction : uint8_t
    {
        SafeState,
        Hold
    };

    struct FaultSettings
    {
        FaultAction action = FaultAction::SafeState;
        uint32_t holdTime = 60;
    };

    FaultSettings faultSettings;

    Regulator();

    void begin(const char* name);
    void begin(
        const char* key,
        const char* name);

    virtual ~Regulator() = default;

    virtual void update(uint32_t now) = 0;

    virtual void resume(uint32_t now);

    double_t readCommand() const;

    bool isCommandValid() const;

    /** Vrai pendant un maintien : la commande est celle d'avant le défaut. */
    bool isInFallback() const;

    /**
     * Consigne active (rampe et programme compris), pour les alarmes
     * relatives à la consigne. Faux si le régulateur n'en a pas, ou s'il
     * est arrêté.
     */
    virtual bool readSetpoint(double_t& setpoint) const
    {
        (void)setpoint;
        return false;
    }

    // ----- Lectures pour la surveillance de boucle (LoopBreakAlarm) -----

    /** Butées de la commande (limites de sortie). */
    virtual void readOutputLimits(
        double_t& minimum,
        double_t& maximum) const
    {
        minimum = 0.0;
        maximum = 1.0;
    }

    /** +1 si la commande fait monter la mesure (chauffage), -1 sinon. */
    virtual int8_t actionDirection() const
    {
        return 1;
    }

    /** Vrai en régulation automatique : ni manuel, ni autotune, ni arrêt. */
    virtual bool isAutomatic() const
    {
        return true;
    }

    /** Temps intégral en secondes, 0 si le régulateur n'en a pas. */
    virtual double_t integralTime() const
    {
        return 0.0;
    }

    /**
     * Relie une alarme qui peut verrouiller ce régulateur : tant qu'elle le
     * demande (Alarm::locksOutputs()), sa commande est invalide et ses
     * sorties sont en état sûr. Sans alarme reliée, aucun effet.
     */
    void setInterlock(const Alarm& alarm);

    bool isInterlocked() const;

    /**
     * Impose l'action en cas de défaut et la retire du menu, par exemple
     * l'état sûr pour un appoint électrique.
     */
    void lockFaultAction(FaultAction action);

    double_t printValue() const override;

    const char* getUnit() const override;

    /**
     * Vrai si la régulation dépend de l'heure du DS3231. Une heure inconnue
     * affiche alors une alerte à la place de l'écran d'accueil.
     */
    virtual bool requiresClock() const
    {
        return false;
    }

    void registerParameters(
        ParameterList& list) override
    {
        (void)list;
    }

protected:

    void writeCommand(double_t value);

    void invalidateCommand();

    /**
     * À appeler à la place de invalidateCommand() quand la mesure d'entrée
     * n'est pas exploitable : applique faultSettings. NotReady (pas encore
     * de mesure) donne toujours l'état sûr.
     */
    void handleMeasurementFault(
        uint32_t now,
        MeasurementStatus status);

    /** Ajoute le réglage du défaut au menu du régulateur, s'il n'est pas verrouillé. */
    void registerFaultParameters(ParameterList& list);

    double_t command = 0;
    bool commandValid = false;

private:
    const Alarm* interlock = nullptr;

    bool faultActionLocked = false;

    // Défaut en cours, et commande d'avant le défaut si elle était valide.
    bool faultActive = false;
    bool holding = false;
    bool holdAvailable = false;
    double_t holdCommand = 0.0;
    uint32_t faultStart = 0;
};

#endif
