#ifndef HEATING_CURVE_H
#define HEATING_CURVE_H

#include <Regulator/Regulator.h>
#include <Regulator/ScheduledSetpoint.h>

class Measurement;
class TimeSchedule;

/**
 * Loi d'eau : température de départ d'un circuit de chauffage calculée
 * d'après la température extérieure.
 *
 * - Consigne (readSetpoint()) : température de départ, à suivre par un PID
 *   ou un thermostat (followSetpoint()). Faux quand la chauffe est arrêtée.
 * - Commande : demande de chauffe, 1 = chauffer (pompe en marche), 0 = arrêt
 *   commandé (été, hors plage en « Arrêt »). Elle peut piloter la pompe,
 *   directement ou par une DelayTimer (post-circulation).
 *
 * Courbe en deux points (froid, doux), définie pour 20 °C d'ambiance,
 * prolongée au-delà et bornée par Départ mini / maxi. Une autre consigne
 * d'ambiance la décale de (1 + pente) × écart. Programme horaire optionnel :
 * consigne d'ambiance réduite, ou arrêt, hors plage.
 *
 * Température extérieure filtrée (inertie du bâtiment) pour la courbe et
 * l'arrêt été (reprise 1 K en dessous). Hors-gel : chauffe arrêtée et
 * extérieur (non filtré) sous le seuil, la demande est maintenue au départ
 * mini.
 *
 * Sonde extérieure en défaut : la courbe utilise T. ext. secours. C'est
 * voulu : arrêter un chauffage en hiver sur une sonde coupée expose au gel.
 * Avant la première mesure, et heure inconnue avec un programme : commande
 * invalide (état sûr).
 */
class HeatingCurve : public Regulator
{
public:
    enum class State : uint8_t
    {
        Waiting,        // pas encore de mesure extérieure
        Comfort,
        Reduced,
        Summer,
        Off,            // hors plage en « Arrêt »
        Frost,
        ClockInvalid
    };

    struct Settings
    {
        // Consigne d'ambiance (confort), en °C.
        double_t roomSetpoint = 20.0;

        // Points de la courbe : température extérieure → départ.
        double_t coldOutdoor = -10.0;
        double_t coldFlow = 45.0;
        double_t mildOutdoor = 20.0;
        double_t mildFlow = 20.0;

        double_t minFlow = 20.0;
        double_t maxFlow = 50.0;

        // Arrêt été (extérieur filtré) et hors-gel (extérieur mesuré).
        double_t summerLimit = 17.0;
        double_t frostLimit = 3.0;

        // Extérieur pris par la courbe quand la sonde est en défaut.
        double_t fallbackOutdoor = 0.0;

        // Constante de temps du filtre de l'extérieur, en heures (0 = aucun).
        uint32_t buildingTimeConstant = 0;
    };

    /** Ambiance de référence des points de la courbe. */
    static constexpr double_t REFERENCE_ROOM = 20.0;

    Settings settings;
    ScheduledSetpoint scheduledSetpoint;

    void begin(
        const char* key,
        const char* name,
        Measurement& outdoor);

    /**
     * Option : consigne d'ambiance réduite (ou arrêt, réglable) hors des
     * plages du programme. À appeler après begin(), avant l'enregistrement
     * des paramètres ; le programme est ajouté au ProcessControl avant.
     */
    void setSchedule(
        const TimeSchedule& schedule,
        double_t reducedRoomSetpoint);

    void update(uint32_t now) override;

    /** Départ calculé quand la chauffe est demandée. */
    bool readSetpoint(double_t& setpoint) const override;

    bool requiresClock() const override;

    /**
     * Départ pour un extérieur et une ambiance donnés, bornes comprises.
     * Réglages de la courbe incohérents : départ mini.
     */
    double_t flowFor(
        double_t outdoor,
        double_t room) const;

    /** Extérieur pris par la courbe : filtré, ou secours en défaut. */
    double_t outdoorTemperature() const;

    /** Vrai si la sonde extérieure est en défaut (secours utilisé). */
    bool isOutdoorFallback() const;

    State state() const;

    static const char* stateName(State state);

    void registerParameters(ParameterList& list) override;

    /** Points froid et doux distincts, départ mini < maxi. */
    bool validateParameters(
        const ParameterEditor& editor) const override;

    void print(Stream& stream) const override;

    double_t printValue() const override;

    const char* getUnit() const override;

private:
    Measurement* outdoor = nullptr;

    double_t flowSetpoint = 0.0;

    double_t filtered = 0.0;
    bool filterStarted = false;
    uint32_t filterTime = 0;

    double_t usedOutdoor = 0.0;
    bool fallback = false;
    bool summer = false;

    State currentState = State::Waiting;

    // Met à jour le filtre ; faux si la sonde est en défaut.
    bool readOutdoor(
        uint32_t now,
        double_t& measured);
};

#endif
