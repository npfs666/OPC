#ifndef LOOP_BREAK_ALARM_H
#define LOOP_BREAK_ALARM_H

#include <Regulator/Alarm.h>

class Measurement;

/**
 * Alarme de boucle ouverte (loop break) : la commande d'un régulateur est
 * en butée et la mesure ne se rapproche pas de la consigne.
 *
 *  - Butée haute (chauffage à 100 %) sans montée : résistance grillée,
 *    contacteur qui ne colle plus, sonde sortie du process.
 *  - Butée basse (chauffage à 0 %) sans descente : contacteur ou SSR collé,
 *    source de chaleur externe.
 *  - En froid, sens inverse.
 *
 * La surveillance s'arme quand la commande est en butée et que l'écart à la
 * consigne dépasse minimumChange. Si la mesure ne s'est pas rapprochée de la
 * consigne d'au moins minimumChange pendant le temps de détection, l'alarme
 * se déclenche. Pas de surveillance en manuel, en autotune, régulateur
 * arrêté ou sur défaut de sonde.
 *
 * Optionnelle : seule une installation qui la déclare l'utilise, et elle est
 * inactive tant que settings.enabled est faux.
 */
class LoopBreakAlarm : public Alarm
{
public:
    struct Settings
    {
        bool enabled = false;

        // Temps de détection en secondes ; 0 = automatique : 2 × Ti du
        // régulateur (au moins 60 s), 600 s s'il n'a pas de Ti.
        uint32_t detectionTime = 0;

        // Rapprochement attendu de la mesure pendant le temps de détection,
        // et bande autour de la consigne sans surveillance.
        double_t minimumChange = 2.0;

        // Un défaut de boucle demande une intervention.
        bool latching = true;

        // Sorties du régulateur en sécurité tant que l'alarme est signalée
        // (en régulation automatique : le mode manuel garde la main).
        bool safeState = true;
    };

    Settings settings;

    static constexpr uint32_t AUTOMATIC_MINIMUM_S = 60;
    static constexpr uint32_t DEFAULT_DETECTION_S = 600;

    /**
     * Surveille regulator, dont la mesure d'entrée est measurement, et le
     * verrouille quand settings.safeState est vrai.
     */
    void begin(
        const char* key,
        const char* name,
        Measurement& measurement,
        Regulator& regulator);

    bool isEnabled() const override;

    bool locksOutputs() const override;

    /** Temps de détection effectif, réglage automatique compris. */
    uint32_t detectionTimeMs() const;

    void update(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

private:
    Measurement* measurement = nullptr;
    const Regulator* regulator = nullptr;

    bool started = false;
    // Butée surveillée : +1 haute, -1 basse, 0 aucune.
    int8_t armedSaturation = 0;
    uint32_t windowStart = 0;
    double_t windowValue = 0.0;

    // Butée à surveiller dans l'état actuel, 0 si aucune.
    int8_t saturation(double_t value, double_t setpoint) const;
};

#endif
