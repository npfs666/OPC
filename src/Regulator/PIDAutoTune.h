#ifndef PID_AUTO_TUNE_H
#define PID_AUTO_TUNE_H

#include <cmath>
#include <cstdint>

/**
 * Machine à états d'un autotune PID par relais.
 *
 * Cette classe ne connaît ni les capteurs, ni les sorties, ni le menu. Elle
 * reçoit des échantillons horodatés et expose uniquement la commande d'essai
 * et le résultat calculé.
 */
class PIDAutoTune
{
public:
    static constexpr uint8_t MAX_CYCLES = 6;

    enum class ProcessDirection : uint8_t
    {
        Invalid = 0,
        OutputRaisesInput = 1,
        OutputLowersInput = 2
    };

    /**
     * Règle de calcul des gains à partir du gain et de la période
     * critiques (Ku, Tu). Valeurs explicites : elles sont persistantes.
     */
    enum class TuningRule : uint8_t
    {
        // Kp = 0,6 Ku, Ti = Tu/2, Td = Tu/8 : rapide, ~25 % de dépassement.
        ZieglerNichols = 0,
        // Kp = Ku/2,2, Ti = 2,2 Tu, Td = Tu/6,3 : prudent, process lents.
        TyreusLuyben = 1,
        // Kp = Ku/3, Ti = Tu/2, Td = Tu/3 : dépassement réduit.
        SomeOvershoot = 2,
        // Kp = Ku/5, Ti = Tu/2, Td = Tu/3 : dépassement quasi nul.
        NoOvershoot = 3
    };

    struct Settings
    {
        double_t outputLow = 0.0;
        double_t outputHigh = 1.0;
        double_t noiseBand = 0.5;

        double_t inputMin = -50.0;
        double_t inputMax = 250.0;

        uint32_t timeoutSeconds = 7200;
        uint32_t minimumCycleSeconds = 10;
        double_t stabilityTolerance = 0.20;
        uint8_t cycles = 3;

        TuningRule rule = TuningRule::TyreusLuyben;
    };

    enum class Status : uint8_t
    {
        Idle,
        WaitingForMeasurement,
        Running,
        Succeeded,
        Failed,
        Cancelled
    };

    enum class Error : uint8_t
    {
        None,
        InvalidSettings,
        InvalidMeasurement,
        InputOutOfRange,
        Timeout,
        InsufficientOscillation,
        TuningsOutOfRange,
        Interrupted
    };

    /** Gains sous forme standard : Kp, temps intégral et dérivé (s). */
    struct Result
    {
        double_t ultimateGain = 0.0;
        double_t ultimatePeriodSeconds = 0.0;

        double_t kp = 0.0;
        double_t ti = 0.0;
        double_t td = 0.0;
    };

    void reset();

    bool start(
        uint32_t now,
        const Settings& settings,
        double_t setpoint,
        double_t outputMinimum,
        double_t outputMaximum,
        ProcessDirection direction =
            ProcessDirection::OutputRaisesInput);

    void cancel(Error error = Error::None);
    void rejectTunings();

    void update(
        uint32_t now,
        bool measurementValid,
        double_t processValue);

    bool isActive() const;
    bool hasCommand() const;
    double_t readCommand() const;

    Status getStatus() const;
    Error getError() const;
    uint8_t getCompletedCycles() const;
    const Result& getResult() const;

    /**
     * Cohérence propre des réglages d'essai (bornes, durées, règle), sans
     * le PID : vérifiée à la validation du menu.
     */
    static bool settingsAreConsistent(const Settings& settings);

    /**
     * Réglages cohérents et compatibles avec le PID : consigne à l'intérieur
     * de la plage de mesure, sorties dans les limites. Vérifié au lancement.
     */
    static bool settingsAreValid(
        const Settings& settings,
        double_t setpoint,
        double_t outputMinimum,
        double_t outputMaximum);

    static bool ruleIsSupported(TuningRule rule);

    /**
     * Calcule kp, ti et td de result à partir de Ku et Tu.
     * Retourne false si la règle ou les valeurs sont invalides.
     */
    static bool applyTuningRule(
        TuningRule rule,
        double_t ultimateGain,
        double_t ultimatePeriodSeconds,
        Result& result);

private:
    static constexpr uint8_t MAX_PEAKS =
        MAX_CYCLES + 1;

    Settings activeSettings;
    double_t activeSetpoint = 0.0;
    ProcessDirection activeDirection =
        ProcessDirection::OutputRaisesInput;

    Status status = Status::Idle;
    Error error = Error::None;
    Result result;

    uint32_t startedAt = 0;
    bool candidateOscillationSeen = false;

    bool outputHigh = false;
    bool commandValid = false;
    double_t command = 0.0;

    double_t phaseExtreme = 0.0;
    uint32_t phaseExtremeAt = 0;

    double_t highPeaks[MAX_PEAKS] = {};
    uint32_t highPeakTimes[MAX_PEAKS] = {};

    double_t lowPeaks[MAX_PEAKS] = {};
    uint32_t lowPeakTimes[MAX_PEAKS] = {};

    uint8_t highPeakCount = 0;
    uint8_t lowPeakCount = 0;

    double_t normalizeInput(
        double_t processValue) const;

    void clearMeasurements();

    void beginOscillation(
        uint32_t now,
        double_t processValue);

    void updateOscillation(
        uint32_t now,
        double_t processValue);

    void recordHighPeak();
    void recordLowPeak();

    void appendPeak(
        double_t value,
        uint32_t time,
        double_t* values,
        uint32_t* times,
        uint8_t& count);

    bool tryFinish();

    bool peaksAreStable(
        double_t averageHighPeak,
        double_t averageLowPeak,
        double_t averagePeriodMs) const;

    void fail(Error failure);
};

#endif
