#ifndef SCHEDULED_SETPOINT_H
#define SCHEDULED_SETPOINT_H

#include <Arduino.h>
#include <hmi/ParameterList.h>

class TimeSchedule;

/**
 * Option de programmation horaire d'un régulateur à consigne.
 *
 * Pendant une plage du programme, la consigne normale (confort) s'applique.
 * Hors plage : consigne réduite, ou régulation arrêtée (arrêt commandé : le
 * régulateur écrit une commande 0 valide, ce n'est pas un défaut).
 * Sans programme attaché, la consigne normale s'applique toujours.
 *
 * Heure inconnue en mode Auto : régulation arrêtée. L'écran d'accueil affiche
 * alors une alerte horloge.
 */
class ScheduledSetpoint
{
public:
    enum class Outside : uint8_t
    {
        Reduced,
        Off
    };

    enum class State : uint8_t
    {
        Unscheduled,
        Comfort,
        Reduced,
        Off,
        ClockInvalid
    };

    struct Settings
    {
        double_t reducedSetpoint = 16.0;
        Outside outside = Outside::Reduced;
    };

    Settings settings;

    /** Retire le programme : la consigne normale s'applique toujours. */
    void begin();

    void attach(
        const TimeSchedule& schedule,
        double_t reducedSetpoint);

    bool isAttached() const;

    /**
     * Met à jour l'état et donne la consigne à appliquer.
     * Retourne false si la régulation doit être arrêtée.
     */
    bool update(
        double_t setpoint,
        double_t& target);

    /**
     * Comme update(), sans mettre à jour l'état : consigne programmée lue
     * par les alarmes relatives, régulateur en manuel.
     */
    bool readTarget(
        double_t setpoint,
        double_t& target) const;

    State state() const;

    static const char* stateName(State state);

    /** N'ajoute rien si aucun programme n'est attaché. */
    bool registerParameters(
        ParameterList::Writer& parameters,
        double_t minimum,
        double_t maximum,
        double_t step,
        uint8_t decimals,
        const char* unit);

private:
    /** État du programme à l'instant présent, et consigne à appliquer. */
    State evaluate(
        double_t setpoint,
        double_t& target) const;

    const TimeSchedule* schedule = nullptr;
    State currentState = State::Unscheduled;
};

#endif
