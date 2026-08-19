#ifndef PID_H
#define PID_H

#include <Regulator/PIDAutoTune.h>
#include <Regulator/Regulator.h>
#include <Regulator/SetpointRamp.h>

class Measurement;

class PID : public Regulator
{
public:
    enum class Mode : uint8_t
    {
        Heating = 0,
        Cooling = 1
    };

    using AutoTuneSettings =
        PIDAutoTune::Settings;

    using AutoTuneStatus =
        PIDAutoTune::Status;

    using AutoTuneError =
        PIDAutoTune::Error;

    using AutoTuneResult =
        PIDAutoTune::Result;

    static constexpr uint8_t MAX_AUTOTUNE_CYCLES =
        PIDAutoTune::MAX_CYCLES;

    struct Settings
    {
        double_t setpoint = 0.0;

        double_t kp = 1.0;
        double_t ki = 0.0;
        double_t kd = 0.0;

        double_t outputMin = 0.0;
        double_t outputMax = 1.0;

        /* Placé à la fin pour préserver les initialisations agrégées. */
        Mode mode = Mode::Heating;

        /** Régulation automatique, indépendante d'un autotune en cours. */
        bool enabled = true;
    };

    Settings settings;
    AutoTuneSettings autoTuneSettings;
    SetpointRamp setpointRamp;

    PID();

    void begin(
        const char* name,
        Measurement& measurement);

    void begin(
        const char* key,
        const char* name,
        Measurement& measurement);

    void reset();

    /** Active la régulation PID automatique. */
    void start();

    /** Arrête toute commande, y compris un autotune en cours. */
    void stop();

    bool isEnabled() const;

    /** Change le sens d'action et réinitialise l'état dynamique du PID. */
    bool setMode(Mode mode);

    bool setTunings(
        double_t kp,
        double_t ki,
        double_t kd);

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

private:
    Measurement* measurement = nullptr;

    double_t integral = 0.0;
    double_t previousMeasurement = 0.0;
    uint32_t previousTime = 0;
    bool initialized = false;

    bool autoTuneParametersRegistered = false;
    bool autoTuneTuningsApplied = false;

    const char* autoTuneOwnerKey = nullptr;

    PIDAutoTune autoTune;

    void resetController();

    bool controlSettingsAreValid() const;

    void updateAutomatic(
        uint32_t now,
        double_t processValue,
        double_t activeSetpoint);
};

#endif
