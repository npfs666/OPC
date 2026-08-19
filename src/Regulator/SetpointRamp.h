#ifndef SETPOINT_RAMP_H
#define SETPOINT_RAMP_H

#include <Arduino.h>

class ParameterList;

/**
 * Limite la vitesse de variation d'une consigne.
 *
 * La consigne active part de la mesure lors de l'activation. Une rampe
 * désactivée suit immédiatement la consigne cible.
 */
class SetpointRamp
{
public:
    struct Settings
    {
        bool enabled = false;
        double_t risingRate = 1.0;
        double_t fallingRate = 1.0;
    };

    Settings settings;

    void begin();

    /**
     * Met à jour la consigne active.
     *
     * Les vitesses sont exprimées dans l'unité de la consigne par minute.
     * Retourne false si une valeur ou un réglage est invalide.
     */
    bool update(
        uint32_t now,
        double_t target,
        double_t processValue);

    /** Fige le temps de rampe pendant une pause ou une mesure invalide. */
    void resume(uint32_t now);

    /** Force une réinitialisation depuis la prochaine mesure valide. */
    void restart();

    double_t activeSetpoint() const;
    bool hasActiveSetpoint() const;

    bool registerParameters(
        ParameterList& list,
        const char* ownerKey,
        const char* ownerName,
        const char* unit);

private:
    double_t active = 0.0;
    uint32_t previousTime = 0;
    bool initialized = false;
    bool activeValid = false;

    bool settingsAreValid() const;
};

#endif
