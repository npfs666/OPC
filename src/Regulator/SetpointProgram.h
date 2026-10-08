// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETPOINT_PROGRAM_H
#define SETPOINT_PROGRAM_H

#include <Regulator/Regulator.h>

class Measurement;

/**
 * Programme de consigne en paliers et rampes (four céramique, étuve,
 * recuit...). Plusieurs programmes enregistrés, chacun de 1 à 8 segments :
 * rampe jusqu'à une cible à une vitesse donnée, puis palier.
 *
 * - Consigne (readSetpoint()) : consigne du programme, à suivre par un PID
 *   ou un thermostat (followSetpoint()). Faux hors cuisson.
 * - Commande : 1 pendant le programme (rampe, palier, maintien final), 0
 *   sinon. Toujours valide : hors cuisson, le régulateur suiveur est à
 *   l'arrêt commandé, ce n'est pas un défaut.
 *
 * La consigne part de la mesure au démarrage. Vitesse 0 : pleine
 * puissance, la consigne passe à la cible et le palier commence quand la
 * mesure l'atteint. Une rampe peut descendre (refroidissement contrôlé ;
 * vitesse 0 : refroidissement libre).
 *
 * Écart maxi (attente garantie, 0 = sans) : tant que la mesure s'écarte de la
 * consigne de plus de cet écart, la rampe s'arrête et le temps de palier ne
 * compte pas. Un four en retard ne raccourcit donc pas le palier.
 *
 * Le temps du programme est figé pendant une pause (menu, timeout de
 * mesure), une inhibition et une mesure invalide ; le régulateur suiveur
 * applique sa propre règle de défaut. Le programme ne dépend pas de
 * l'horloge. Il n'est pas sauvegardé : après une coupure de courant, il est
 * arrêté.
 */
class SetpointProgram : public Regulator
{
public:
    static constexpr uint8_t MAX_PROGRAMS = 4;
    static constexpr uint8_t MAX_SEGMENTS = 8;

    /** Après le dernier segment : arrêt, ou maintien de la dernière cible. */
    enum class End : uint8_t
    {
        Stop,
        Hold
    };

    enum class State : uint8_t
    {
        Idle,           // pas de programme lancé
        Delayed,        // départ différé en cours
        Waiting,        // départ demandé, attente d'une mesure valide
        Ramp,
        Soak,
        Hold,           // maintien de la dernière cible, sans fin
        Finished
    };

    struct Segment
    {
        // Unité de la mesure par heure ; 0 = pleine puissance.
        double_t rate = 0.0;
        double_t target = 0.0;
        // Palier, en minutes.
        uint32_t soak = 0;
    };

    struct Program
    {
        uint8_t segmentCount = 1;
        Segment segments[MAX_SEGMENTS];
    };

    struct Settings
    {
        // Programme lancé par start(), de 1 au nombre de programmes.
        uint8_t selected = 1;
        // Départ différé, en minutes.
        uint32_t startDelay = 0;
        // Écart maxi (attente garantie), 0 = sans.
        double_t holdback = 0.0;
        End end = End::Stop;
        Program programs[MAX_PROGRAMS];
    };

    Settings settings;

    /**
     * programCount : nombre de programmes proposés au menu (1 à
     * MAX_PROGRAMS). key sert aussi de préfixe aux programmes ("<key>.p1")
     * et aux segments ("<key>.p1.s1") : la garder courte (moins de
     * 30 caractères).
     */
    void begin(
        const char* key,
        const char* name,
        Measurement& measurement,
        uint8_t programCount = 1);

    /**
     * Plage de réglage des cibles (défaut 0 à 1300) et vitesse maxi (défaut
     * 999 par heure). À appeler après begin(), avant l'enregistrement des
     * paramètres.
     */
    bool setLimits(
        double_t targetMinimum,
        double_t targetMaximum,
        double_t rateMaximum);

    /** Unités affichées au menu (défaut "°C" et "°C/h"). */
    void setUnits(
        const char* unit,
        const char* rateUnit);

    uint8_t programCount() const;

    /**
     * Lance le programme sélectionné (settings.selected), après le départ
     * différé. Faux si la mesure est invalide ou le programme inexistant :
     * rien ne démarre. Relance depuis le début un programme en cours.
     */
    bool start();

    /** Arrête le programme : arrêt commandé du régulateur suiveur. */
    void stop();

    /**
     * Termine le segment en cours (rampe et palier) ; pendant le départ
     * différé, démarre tout de suite. Faux s'il n'y a rien à passer.
     */
    bool skipSegment();

    /** Programme lancé : départ différé, attente, rampe, palier, maintien. */
    bool isRunning() const;

    State state() const;

    static const char* stateName(State state);

    /** Programme lancé (1 à N), 0 si aucun. */
    uint8_t runningProgram() const;

    /** Segment en cours (1 à N), 0 si aucun. */
    uint8_t segment() const;

    /** Nombre de segments du programme lancé, 0 si aucun. */
    uint8_t segmentCount() const;

    /** Cible du segment en cours. Faux hors rampe et palier. */
    bool readSegmentTarget(double_t& target) const;

    /** Temps figé par l'écart maxi. */
    bool isHeldBack() const;

    /**
     * Rampe descendante en cours (refroidissement contrôlé ou libre) : la
     * mesure au-dessus de la consigne y est normale.
     */
    bool isCooling() const;

    /** Temps de programme écoulé depuis la fin du départ différé. */
    uint32_t elapsedSeconds() const;

    /** Reste du départ différé, 0 en dehors. */
    uint32_t delayRemainingSeconds() const;

    /** Reste du palier en cours, 0 en dehors. */
    uint32_t soakRemainingSeconds() const;

    /**
     * Estimation du temps restant jusqu'à la fin du programme, aux vitesses
     * réglées et sans attente. Un segment à pleine puissance compte pour sa
     * seule durée de palier. 0 hors rampe et palier.
     */
    uint32_t remainingSeconds() const;

    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    bool readSetpoint(double_t& setpoint) const override;

    void registerParameters(ParameterList& list) override;

    void print(Stream& stream) const override;

    double_t printValue() const override;

    const char* getUnit() const override;

private:
    static constexpr size_t KEY_LENGTH = 40;

    Measurement* measurement = nullptr;

    uint8_t programs = 1;

    double_t targetMinimum = 0.0;
    double_t targetMaximum = 1300.0;
    double_t rateMaximum = 999.0;

    const char* unit = "°C";
    const char* rateUnit = "°C/h";

    char programKeys[MAX_PROGRAMS][KEY_LENGTH] = {};
    char segmentKeys[MAX_PROGRAMS][MAX_SEGMENTS][KEY_LENGTH] = {};

    State currentState = State::Idle;

    // Programme et segment en cours, indices depuis 0.
    uint8_t programIndex = 0;
    uint8_t segmentIndex = 0;

    double_t setpoint = 0.0;
    // Consigne au début de la rampe : sens de la rampe à pleine puissance.
    double_t segmentStart = 0.0;
    bool heldBack = false;

    uint32_t lastTime = 0;
    bool timeKnown = false;

    uint32_t delayElapsedMs = 0;
    uint32_t soakElapsedMs = 0;
    uint32_t runElapsedMs = 0;

    const Program& runningSettings() const;

    // Rampe, palier ou maintien : la consigne est donnée.
    bool hasSetpoint() const;

    bool readMeasurement(double_t& value) const;

    // Hors de l'écart maxi autour de reference (écart maxi réglé).
    bool outsideHoldback(
        double_t measured,
        double_t reference) const;

    void enterSegment(uint8_t index);

    // Segment terminé : suivant, ou fin du programme.
    void nextSegment();

    // Fin du programme : maintien ou arrêt, selon le réglage Fin.
    void finish();

    void updateRamp(
        uint32_t elapsedMs,
        double_t measured);

    void updateSoak(
        uint32_t elapsedMs,
        double_t measured);
};

#endif
