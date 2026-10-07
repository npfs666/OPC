#ifndef LIMIT_ALARM_H
#define LIMIT_ALARM_H

#include <Regulator/Alarm.h>

class Measurement;

/**
 * Surveillance d'une mesure (alarme de seuil), sur le modèle des fonctions
 * d'alarme des régulateurs compacts. Voir Alarm pour l'affichage, la
 * mémorisation et l'acquittement.
 *
 * Types : Max et Min comparent la mesure à un seuil absolu ; Ecart haut,
 * Ecart bas et Hors bande la comparent à la consigne active d'un régulateur
 * de référence (setReference()), décalée de l'écart réglé.
 */
class LimitAlarm : public Alarm
{
public:
    enum class Type : uint8_t
    {
        Max,
        Min,
        DeviationHigh,
        DeviationLow,
        Band
    };

    struct Settings
    {
        bool enabled = false;
        Type type = Type::Max;

        // Seuil absolu (Max, Min) ou écart à la consigne (autres types).
        double_t limit = 100.0;

        // L'alarme cesse quand la mesure revient de cette valeur en deçà.
        double_t hysteresis = 1.0;

        // Durée minimale au-delà du seuil avant l'alarme, en secondes.
        uint32_t delay = 0;

        // Alarmes basses (Min, Ecart bas, Hors bande sous la consigne) : pas
        // d'alarme tant que la mesure n'est pas d'abord entrée dans la zone
        // normale (mise en chauffe, activation, consigne retrouvée). Sans
        // effet sur un dépassement haut, toujours signalé.
        bool startupMasking = true;

        // L'alarme reste signalée jusqu'à l'acquittement.
        bool latching = false;

        // Un défaut de sonde (rupture, court-circuit, hors étendue) déclenche
        // l'alarme.
        bool alarmOnFault = true;
    };

    Settings settings;

    LimitAlarm();

    void begin(
        const char* key,
        const char* name,
        Measurement& measurement);

    /**
     * Régulateur dont la consigne active sert aux types relatifs. Sans
     * référence, seuls Max et Min sont proposés. À appeler après begin(),
     * avant l'enregistrement des paramètres.
     */
    void setReference(const Regulator& regulator);

    bool isEnabled() const override;

    void update(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor) const override;

private:
    Measurement* measurement = nullptr;
    const Regulator* reference = nullptr;

    bool started = false;       // état initialisé depuis l'activation
    bool beyond = false;        // seuil franchi (hystérésis comprise)
    bool masked = false;
    bool pending = false;       // condition présente, temporisation en cours
    uint32_t pendingSince = 0;

    // Tempo écoulée : acquise tant que la condition dure (now - pendingSince
    // reboucle après 49 jours et n'est plus consulté).
    bool delayElapsed = false;

    bool isRelative(Type type) const;

    // Seuil franchi, avec hystérésis à partir de l'état précédent.
    bool isBeyond(double_t value, double_t setpoint) const;

    void restart();
};

#endif
