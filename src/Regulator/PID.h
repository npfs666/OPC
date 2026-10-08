#ifndef PID_H
#define PID_H

#include <Regulator/PIDAutoTune.h>
#include <Regulator/Regulator.h>
#include <Regulator/ScheduledSetpoint.h>
#include <Regulator/SetpointRamp.h>

class Measurement;
class TimeSchedule;

class PID : public Regulator
{
public:
    enum class Mode : uint8_t
    {
        Heating = 0,
        Cooling = 1
    };

    /** Commande automatique, ou sortie fixée par l'opérateur. */
    enum class Operation : uint8_t
    {
        Auto = 0,
        Manual = 1
    };

    using AutoTuneSettings =
        PIDAutoTune::Settings;

    using AutoTuneStatus =
        PIDAutoTune::Status;

    using AutoTuneError =
        PIDAutoTune::Error;

    using AutoTuneResult =
        PIDAutoTune::Result;

    using AutoTuneRule =
        PIDAutoTune::TuningRule;

    static constexpr uint8_t MAX_AUTOTUNE_CYCLES =
        PIDAutoTune::MAX_CYCLES;

    /**
     * Forme standard (ISA) :
     * u = Kp · (e + 1/Ti · ∫e dt + Td · d(-mesure)/dt)
     * La dérivée porte sur la mesure et passe par un filtre de
     * constante de temps Td / DERIVATIVE_FILTER_RATIO.
     */
    struct Settings
    {
        double_t setpoint = 0.0;

        /** Gain proportionnel, en fraction de sortie par unité de mesure. */
        double_t kp = 1.0;
        /** Temps intégral en secondes. 0 désactive l'action intégrale. */
        double_t ti = 0.0;
        /** Temps dérivé en secondes. 0 désactive l'action dérivée. */
        double_t td = 0.0;

        double_t outputMin = 0.0;
        double_t outputMax = 1.0;

        /* Placé à la fin pour préserver les initialisations agrégées. */
        Mode mode = Mode::Heating;

        /** Régulation automatique, indépendante d'un autotune en cours. */
        bool enabled = true;

        /*
         * Mode manuel : la sortie vaut manualOutput, sans tenir compte de la
         * mesure ni de enabled. Non sauvegardé : retour en Auto au démarrage.
         */
        Operation operation = Operation::Auto;

        /** Sortie manuelle en %. En Auto, elle suit la sortie calculée. */
        double_t manualOutput = 0.0;
    };

    Settings settings;
    AutoTuneSettings autoTuneSettings;
    SetpointRamp setpointRamp;
    ScheduledSetpoint scheduledSetpoint;

    PID();

    void begin(
        const char* name,
        Measurement& measurement);

    void begin(
        const char* key,
        const char* name,
        Measurement& measurement);

    void reset();

    /**
     * Option : consigne pendant les plages du programme, reducedSetpoint
     * (ou arrêt, réglable) en dehors. Un autotune ignore le programme.
     * À appeler après begin(), avant l'enregistrement des paramètres. Le
     * programme doit aussi être ajouté au ProcessControl.
     */
    void setSchedule(
        const TimeSchedule& schedule,
        double_t reducedSetpoint);

    /**
     * Consigne d'exécution : la consigne est celle de source
     * (readSetpoint()), recalculée à chaque cycle et jamais sauvegardée
     * (loi d'eau...). Source sans consigne et commande valide : arrêt
     * commandé (commande 0 valide). Commande de la source invalide : état
     * sûr. Les réglages « Consigne » et le programme du PID quittent le
     * menu. La source doit être ajoutée au ProcessControl avant le PID.
     * À appeler après begin(), avant l'enregistrement des paramètres.
     */
    void followSetpoint(const Regulator& source);

    /** Vrai si la consigne vient d'un autre régulateur. */
    bool followsSetpoint() const;

    /** Active la régulation PID automatique. */
    void start();

    /** Arrête toute commande, y compris un autotune en cours. */
    void stop();

    bool isEnabled() const;

    /** Change le sens d'action et réinitialise l'état dynamique du PID. */
    bool setMode(Mode mode);

    /** kp > 0, ti et td en secondes (0 = action désactivée). */
    bool setTunings(
        double_t kp,
        double_t ti,
        double_t td);

    /**
     * Plage de réglage de la consigne dans le menu (défaut -50 à 250).
     * À appeler avant registerParameters().
     */
    bool setSetpointLimits(
        double_t minimum,
        double_t maximum);

    bool setOutputLimits(
        double_t minimum,
        double_t maximum);

    /**
     * Programme un autotune horodaté. La sortie reste sûre jusqu'à la
     * première mesure valide et l'attente est incluse dans le timeout.
     */
    bool startAutoTune(uint32_t now);

    void cancelAutoTune();

    bool isAutoTuneActive() const;

    AutoTuneStatus getAutoTuneStatus() const;
    AutoTuneError getAutoTuneError() const;
    uint8_t getAutoTuneCompletedCycles() const;

    const AutoTuneResult& getAutoTuneResult() const;

    /**
     * Consomme l'événement indiquant que l'autotune vient d'appliquer
     * de nouveaux gains au PID principal.
     */
    bool takeAutoTuneTuningsApplied();

    void update(uint32_t now) override;
    void resume(uint32_t now) override;

    /**
     * Consigne active, sauf PID arrêté ou autotune en cours. En manuel,
     * consigne réglée (voir Regulator::readSetpoint()).
     */
    bool readSetpoint(double_t& setpoint) const override;

    void readOutputLimits(
        double_t& minimum,
        double_t& maximum) const override;

    int8_t actionDirection() const override;

    /** Auto, activé, sans autotune en cours. */
    bool isAutomatic() const override;

    /** Commande Manuel : prioritaire sur l'inhibition. */
    bool isManual() const override;

    double_t integralTime() const override;

    /** Enregistre uniquement les réglages du PID automatique. */
    void registerParameters(
        ParameterList& list) override;

    /**
     * Ajoute les réglages d'autotune sous un propriétaire de menu distinct.
     * Une installation ne l'appelle que lorsqu'elle propose cette fonction.
     */
    bool registerAutoTuneParameters(
        ParameterList& list,
        const char* ownerKey,
        const char* ownerName);

    bool validateParameters(
        const ParameterEditor& editor)
        const override;

    void print(Stream& stream) const override;

    /** Filtre de la dérivée : constante de temps = Td / ce rapport. */
    static constexpr double_t DERIVATIVE_FILTER_RATIO = 10.0;

private:
    Measurement* measurement = nullptr;

    const Regulator* setpointSource = nullptr;

    /**
     * Consigne cible avant rampe : celle de la source, ou la consigne
     * réglée avec son programme. Faux s'il n'y en a pas.
     */
    bool readTarget(double_t& target) const;

    double_t setpointMinimum = -50.0;
    double_t setpointMaximum = 250.0;

    /*
     * Terme intégral exprimé en fraction de sortie : un changement de Kp
     * ou de Ti ne fait donc pas sauter la commande.
     */
    double_t integralTerm = 0.0;
    Mode integralMode = Mode::Heating;
    double_t filteredDerivative = 0.0;
    double_t previousMeasurement = 0.0;
    uint32_t previousTime = 0;
    bool initialized = false;

    bool autoTuneParametersRegistered = false;
    bool autoTuneTuningsApplied = false;

    const char* autoTuneOwnerKey = nullptr;

    PIDAutoTune autoTune;

    /** Remet tout l'état dynamique à zéro, intégrale comprise. */
    void resetController();

    /**
     * Suspend la régulation (commande invalide, dérivée réinitialisée) en
     * conservant l'intégrale : reprise sans à-coup après une pause.
     */
    void holdController();

    /** Comme holdController(), sans toucher à la commande. */
    void freezeController();

    /**
     * Arrêt commandé (PID désactivé, hors plage en « Arrêt ») : état remis
     * à zéro, commande 0 valide. Ce n'est pas un défaut : les blocs qui
     * dépendent du PID ne passent pas en état sûr.
     */
    void stopController();

    /*
     * Retour de manuel en automatique : au premier cycle, l'intégrale est
     * recalculée pour que la sortie reprenne la dernière sortie manuelle
     * (mémorisée ici : la validation du menu efface la commande).
     */
    bool manualHandover = false;
    double_t handoverCommand = 0.0;

    void updateManual();
    void updateControl(uint32_t now);

    bool controlSettingsAreValid() const;

    void updateAutomatic(
        uint32_t now,
        double_t processValue,
        double_t activeSetpoint);
};

#endif
