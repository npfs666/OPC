#include <Regulator/PID.h>

#include <Arduino.h>
#include <Measurements/Measurement.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    constexpr double_t KP_MAX = 10.0;
    constexpr double_t KI_MAX = 0.1;
    constexpr double_t KD_MAX = 100.0;

    constexpr ParameterOption PID_MODE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                PID::Mode::Heating),
            "Chaud"
        },
        {
            static_cast<int32_t>(
                PID::Mode::Cooling),
            "Froid"
        }
    };

    bool modeIsSupported(PID::Mode mode)
    {
        return
            mode == PID::Mode::Heating ||
            mode == PID::Mode::Cooling;
    }

    bool tuningsAreSupported(
        double_t kp,
        double_t ki,
        double_t kd)
    {
        return
            std::isfinite(kp) &&
            std::isfinite(ki) &&
            std::isfinite(kd) &&
            kp >= 0.0 && kp <= KP_MAX &&
            ki >= 0.0 && ki <= KI_MAX &&
            kd >= 0.0 && kd <= KD_MAX;
    }

    bool readNumberDraft(
        const ParameterEditor& editor,
        const char* ownerKey,
        const char* parameterKey,
        double_t& value)
    {
        const ParameterDraft* draft =
            editor.find(
                ownerKey,
                parameterKey);

        if (draft == nullptr ||
            draft->parameter == nullptr ||
            draft->parameter->type !=
                Parameter::Type::Double)
        {
            return false;
        }

        value = draft->numberValue;

        return std::isfinite(value);
    }

    bool readIntegerDraft(
        const ParameterEditor& editor,
        const char* ownerKey,
        const char* parameterKey,
        int32_t& value)
    {
        const ParameterDraft* draft =
            editor.find(
                ownerKey,
                parameterKey);

        if (draft == nullptr ||
            draft->parameter == nullptr ||
            draft->parameter->type !=
                Parameter::Type::Integer)
        {
            return false;
        }

        value = draft->integerValue;

        return true;
    }

    const char* autoTuneStatusName(
        PID::AutoTuneStatus status)
    {
        switch (status)
        {
        case PID::AutoTuneStatus::Idle:
            return "idle";

        case PID::AutoTuneStatus::WaitingForMeasurement:
            return "waiting";

        case PID::AutoTuneStatus::Running:
            return "running";

        case PID::AutoTuneStatus::Succeeded:
            return "succeeded";

        case PID::AutoTuneStatus::Failed:
            return "failed";

        case PID::AutoTuneStatus::Cancelled:
            return "cancelled";
        }

        return "unknown";
    }

    const char* autoTuneErrorName(
        PID::AutoTuneError error)
    {
        switch (error)
        {
        case PID::AutoTuneError::None:
            return "none";

        case PID::AutoTuneError::InvalidSettings:
            return "invalid settings";

        case PID::AutoTuneError::InvalidMeasurement:
            return "invalid measurement";

        case PID::AutoTuneError::InputOutOfRange:
            return "input out of range";

        case PID::AutoTuneError::Timeout:
            return "timeout";

        case PID::AutoTuneError::InsufficientOscillation:
            return "unstable oscillation";

        case PID::AutoTuneError::TuningsOutOfRange:
            return "tunings out of range";

        case PID::AutoTuneError::Interrupted:
            return "interrupted";
        }

        return "unknown";
    }
}

PID::PID()
{
}

void PID::begin(
    const char* name,
    Measurement& measurement)
{
    begin(name, name, measurement);
}

void PID::begin(
    const char* key,
    const char* name,
    Measurement& measurement)
{
    Regulator::begin(key, name);

    this->measurement = &measurement;

    settings = Settings{};
    autoTuneSettings = AutoTuneSettings{};
    autoTuneParametersRegistered = false;
    autoTuneTuningsApplied = false;
    autoTuneOwnerKey = nullptr;

    reset();
}

void PID::resetController()
{
    integral = 0.0;
    previousMeasurement = 0.0;
    previousTime = 0;
    initialized = false;

    invalidateCommand();
}

void PID::reset()
{
    settings.enabled = true;
    autoTune.reset();
    autoTuneTuningsApplied = false;
    resetController();
}

void PID::start()
{
    settings.enabled = true;
    autoTune.reset();
    resetController();
}

void PID::stop()
{
    if (autoTune.isActive())
        autoTune.cancel();

    settings.enabled = false;
    resetController();
}

bool PID::isEnabled() const
{
    return
        settings.enabled ||
        autoTune.isActive();
}

bool PID::setMode(Mode mode)
{
    if (autoTune.isActive() ||
        !modeIsSupported(mode))
    {
        return false;
    }

    settings.mode = mode;

    autoTune.reset();
    resetController();

    return true;
}

bool PID::setTunings(
    double_t kp,
    double_t ki,
    double_t kd)
{
    if (autoTune.isActive() ||
        !tuningsAreSupported(
            kp,
            ki,
            kd))
    {
        return false;
    }

    settings.kp = kp;
    settings.ki = ki;
    settings.kd = kd;

    autoTune.reset();
    resetController();

    return true;
}

bool PID::setOutputLimits(
    double_t minimum,
    double_t maximum)
{
    if (autoTune.isActive() ||
        !std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        minimum < 0.0 ||
        maximum > 1.0 ||
        minimum > maximum)
    {
        return false;
    }

    settings.outputMin = minimum;
    settings.outputMax = maximum;

    resetController();

    return true;
}

bool PID::startAutoTune(uint32_t now)
{
    /* L'essai ne doit jamais réactiver le PID automatique au redémarrage. */
    settings.enabled = false;

    PIDAutoTune::ProcessDirection direction =
        PIDAutoTune::ProcessDirection::Invalid;

    if (settings.mode == Mode::Heating)
    {
        direction = PIDAutoTune::ProcessDirection::
            OutputRaisesInput;
    }
    else if (settings.mode == Mode::Cooling)
    {
        direction = PIDAutoTune::ProcessDirection::
            OutputLowersInput;
    }

    bool started = false;

    if (measurement != nullptr)
    {
        started = autoTune.start(
            now,
            autoTuneSettings,
            settings.setpoint,
            settings.outputMin,
            settings.outputMax,
            direction);
    }
    else
    {
        autoTune.reset();
    }

    resetController();

    return started;
}

void PID::cancelAutoTune()
{
    if (!autoTune.isActive())
        return;

    autoTune.cancel();
    settings.enabled = false;
    resetController();
}

bool PID::isAutoTuneActive() const
{
    return autoTune.isActive();
}

PID::AutoTuneStatus PID::getAutoTuneStatus() const
{
    return autoTune.getStatus();
}

PID::AutoTuneError PID::getAutoTuneError() const
{
    return autoTune.getError();
}

uint8_t PID::getAutoTuneCompletedCycles() const
{
    return autoTune.getCompletedCycles();
}

const PID::AutoTuneResult&
PID::getAutoTuneResult() const
{
    return autoTune.getResult();
}

bool PID::takeAutoTuneTuningsApplied()
{
    const bool applied =
        autoTuneTuningsApplied;

    autoTuneTuningsApplied = false;

    return applied;
}

bool PID::controlSettingsAreValid() const
{
    return
        modeIsSupported(settings.mode) &&
        std::isfinite(settings.setpoint) &&
        tuningsAreSupported(
            settings.kp,
            settings.ki,
            settings.kd) &&
        std::isfinite(settings.outputMin) &&
        std::isfinite(settings.outputMax) &&
        settings.outputMin >= 0.0 &&
        settings.outputMax <= 1.0 &&
        settings.outputMin <=
            settings.outputMax;
}

void PID::update(uint32_t now)
{
    const bool measurementValid =
        measurement != nullptr &&
        measurement->isValid() &&
        std::isfinite(
            measurement->getValue());

    const double_t processValue =
        measurementValid
            ? measurement->getValue()
            : 0.0;

    if (autoTune.isActive())
    {
        autoTune.update(
            now,
            measurementValid,
            processValue);

        if (autoTune.getStatus() ==
            AutoTuneStatus::Succeeded)
        {
            const AutoTuneResult& result =
                autoTune.getResult();

            if (tuningsAreSupported(
                    result.kp,
                    result.ki,
                    result.kd))
            {
                settings.kp = result.kp;
                settings.ki = result.ki;
                settings.kd = result.kd;
                autoTuneTuningsApplied = true;
            }
            else
            {
                autoTune.rejectTunings();
            }

            /*
             * Un essai ne démarre jamais l'équipement régulé tout seul.
             * L'utilisateur doit relire les gains puis appeler start().
             */
            settings.enabled = false;
            resetController();
            return;
        }

        if (!autoTune.isActive())
        {
            settings.enabled = false;
            resetController();
            return;
        }

        if (autoTune.hasCommand())
            writeCommand(autoTune.readCommand());
        else
            invalidateCommand();

        return;
    }

    if (!settings.enabled)
    {
        invalidateCommand();
        return;
    }

    if (!measurementValid ||
        !controlSettingsAreValid())
    {
        resetController();
        return;
    }

    updateAutomatic(
        now,
        processValue);
}

void PID::updateAutomatic(
    uint32_t now,
    double_t processValue)
{
    if (settings.ki <= 0.0)
        integral = 0.0;

    if (!initialized)
    {
        previousMeasurement = processValue;
        previousTime = now;
        initialized = true;
        invalidateCommand();
        return;
    }

    const double_t dt =
        static_cast<double_t>(
            now - previousTime) /
        1000.0;

    if (dt <= 0.0)
        return;

    const double_t actionSign =
        settings.mode == Mode::Heating
            ? 1.0
            : -1.0;

    const double_t error =
        actionSign *
        (settings.setpoint -
         processValue);

    const double_t derivative =
        actionSign *
        (-(processValue -
           previousMeasurement) /
         dt);

    const double_t proportionalTerm =
        settings.kp * error;

    const double_t derivativeTerm =
        settings.kd * derivative;

    if (!std::isfinite(proportionalTerm) ||
        !std::isfinite(derivativeTerm))
    {
        resetController();
        return;
    }

    if (settings.ki > 0.0)
    {
        const double_t candidateIntegral =
            integral + error * dt;

        const double_t candidateOutput =
            proportionalTerm +
            settings.ki * candidateIntegral +
            derivativeTerm;

        const bool outputInsideLimits =
            candidateOutput >=
                settings.outputMin &&
            candidateOutput <=
                settings.outputMax;

        const bool unwindsHighSaturation =
            candidateOutput >
                settings.outputMax &&
            error < 0.0;

        const bool unwindsLowSaturation =
            candidateOutput <
                settings.outputMin &&
            error > 0.0;

        if (std::isfinite(candidateIntegral) &&
            std::isfinite(candidateOutput) &&
            (outputInsideLimits ||
             unwindsHighSaturation ||
             unwindsLowSaturation))
        {
            integral = candidateIntegral;
        }
    }

    double_t output =
        proportionalTerm +
        settings.ki * integral +
        derivativeTerm;

    if (!std::isfinite(output))
    {
        resetController();
        return;
    }

    output = constrain(
        output,
        settings.outputMin,
        settings.outputMax);

    writeCommand(output);

    previousMeasurement = processValue;
    previousTime = now;
}

void PID::resume(uint32_t now)
{
    (void)now;

    /*
     * WaitingForMeasurement peut avoir été demandé depuis le menu pendant
     * la pause d'acquisition : il doit atteindre la première mesure après
     * la reprise. Seul un essai déjà Running a réellement été interrompu.
     */
    if (autoTune.getStatus() ==
        AutoTuneStatus::Running)
    {
        autoTune.cancel(
            AutoTuneError::Interrupted);
        settings.enabled = false;
    }

    resetController();
}

void PID::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    });

    const char* inputUnit =
        measurement != nullptr
            ? measurement->getUnit()
            : nullptr;

    parameters.addBool(
        "enabled",
        "Activé",
        settings.enabled);

    parameters.addSelection(
        "mode",
        "Mode",
        settings.mode,
        PID_MODE_OPTIONS);

    parameters.addDouble(
        "setpoint",
        "Consigne",
        settings.setpoint,
        0.0,
        80.0,
        0.5,
        1,
        inputUnit);

    parameters.addDouble(
        "kp",
        "Kp",
        settings.kp,
        0.0,
        KP_MAX,
        0.01,
        2,
        "1/°C");

    parameters.addDouble(
        "ki",
        "Ki",
        settings.ki,
        0.0,
        KI_MAX,
        0.001,
        3,
        "1/(°C.s)");

    parameters.addDouble(
        "kd",
        "Kd",
        settings.kd,
        0.0,
        KD_MAX,
        0.1,
        1,
        "s/°C");

    parameters.addDouble(
        "output_min",
        "Sortie min",
        settings.outputMin,
        0.0,
        1.0,
        0.01,
        2);

    parameters.addDouble(
        "output_max",
        "Sortie max",
        settings.outputMax,
        0.0,
        1.0,
        0.01,
        2);
}

bool PID::registerAutoTuneParameters(
    ParameterList& list,
    const char* ownerKey,
    const char* ownerName)
{
    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        ownerKey,
        ownerName
    });

    const char* inputUnit =
        measurement != nullptr
            ? measurement->getUnit()
            : nullptr;

    const bool registered =
        parameters.addDouble(
            "autotune_output_low",
            "Sortie basse",
            autoTuneSettings.outputLow,
            0.0,
            1.0,
            0.05,
            2) &&
        parameters.addDouble(
            "autotune_output_high",
            "Sortie haute",
            autoTuneSettings.outputHigh,
            0.0,
            1.0,
            0.05,
            2) &&
        parameters.addDouble(
            "autotune_noise_band",
            "Demi-bande",
            autoTuneSettings.noiseBand,
            0.05,
            10.0,
            0.05,
            2,
            inputUnit) &&
        parameters.addDouble(
            "autotune_input_min",
            "Mesure min",
            autoTuneSettings.inputMin,
            -50.0,
            250.0,
            1.0,
            1,
            inputUnit) &&
        parameters.addDouble(
            "autotune_input_max",
            "Mesure max",
            autoTuneSettings.inputMax,
            -50.0,
            250.0,
            1.0,
            1,
            inputUnit) &&
        parameters.addInteger(
            "autotune_timeout",
            "Timeout",
            autoTuneSettings.timeoutSeconds,
            60,
            86400,
            60,
            "s") &&
        parameters.addInteger(
            "autotune_min_cycle",
            "Période min",
            autoTuneSettings.minimumCycleSeconds,
            1,
            3600,
            1,
            "s") &&
        parameters.addDouble(
            "autotune_stability",
            "Stabilité",
            autoTuneSettings.stabilityTolerance,
            0.05,
            0.50,
            0.05,
            2) &&
        parameters.addInteger(
            "autotune_cycles",
            "Cycles",
            autoTuneSettings.cycles,
            2,
            MAX_AUTOTUNE_CYCLES,
            1);

    autoTuneParametersRegistered = registered;
    autoTuneOwnerKey =
        registered
            ? ownerKey
            : nullptr;

    return registered;
}

bool PID::validateParameters(
    const ParameterEditor& editor) const
{
    const char* pidOwnerKey =
        getConfigurationKey();

    double_t outputMin = 0.0;
    double_t outputMax = 0.0;

    if (!readNumberDraft(
            editor,
            pidOwnerKey,
            "output_min",
            outputMin) ||
        !readNumberDraft(
            editor,
            pidOwnerKey,
            "output_max",
            outputMax) ||
        outputMin > outputMax)
    {
        return false;
    }

    if (!autoTuneParametersRegistered)
        return true;

    if (autoTuneOwnerKey == nullptr)
        return false;

    double_t setpoint = 0.0;
    AutoTuneSettings tuneSettings =
        autoTuneSettings;

    int32_t timeoutSeconds = 0;
    int32_t minimumCycleSeconds = 0;
    int32_t cycles = 0;

    if (!readNumberDraft(
            editor,
            pidOwnerKey,
            "setpoint",
            setpoint) ||
        !readNumberDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_output_low",
            tuneSettings.outputLow) ||
        !readNumberDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_output_high",
            tuneSettings.outputHigh) ||
        !readNumberDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_noise_band",
            tuneSettings.noiseBand) ||
        !readNumberDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_input_min",
            tuneSettings.inputMin) ||
        !readNumberDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_input_max",
            tuneSettings.inputMax) ||
        !readNumberDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_stability",
            tuneSettings.stabilityTolerance) ||
        !readIntegerDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_timeout",
            timeoutSeconds) ||
        !readIntegerDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_min_cycle",
            minimumCycleSeconds) ||
        !readIntegerDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_cycles",
            cycles))
    {
        return false;
    }

    if (timeoutSeconds < 0 ||
        minimumCycleSeconds < 0 ||
        cycles < 0 ||
        cycles > UINT8_MAX)
    {
        return false;
    }

    tuneSettings.timeoutSeconds =
        static_cast<uint32_t>(
            timeoutSeconds);

    tuneSettings.minimumCycleSeconds =
        static_cast<uint32_t>(
            minimumCycleSeconds);

    tuneSettings.cycles =
        static_cast<uint8_t>(cycles);

    return PIDAutoTune::settingsAreValid(
        tuneSettings,
        setpoint,
        outputMin,
        outputMax);
}

void PID::print(Stream& stream) const
{
    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");

    if (isCommandValid())
        stream.print(command);
    else
        stream.print("safe");

    stream.print(" | Measur : ");

    if (measurement != nullptr &&
        measurement->isValid())
    {
        stream.print(
            measurement->printValue(),
            measurement->printDecimals());
        stream.print(
            measurement->getUnit());
    }
    else
    {
        stream.print("invalid");
    }

    stream.print(" | SP : ");
    stream.print(settings.setpoint, 2);

    stream.print(" | Mode : ");
    stream.print(
        settings.mode == Mode::Heating
            ? "heating"
            : settings.mode == Mode::Cooling
                ? "cooling"
                : "invalid");

    const AutoTuneStatus tuneStatus =
        autoTune.getStatus();

    if (!settings.enabled)
        stream.print(" | PID : stopped");

    if (tuneStatus != AutoTuneStatus::Idle)
    {
        stream.print(" | Tune : ");
        stream.print(
            autoTuneStatusName(
                tuneStatus));
    }

    if (tuneStatus == AutoTuneStatus::Failed ||
        (tuneStatus == AutoTuneStatus::Cancelled &&
         autoTune.getError() !=
             AutoTuneError::None))
    {
        stream.print(" (");
        stream.print(
            autoTuneErrorName(
                autoTune.getError()));
        stream.print(')');
    }

    if (tuneStatus == AutoTuneStatus::Succeeded)
    {
        stream.print(" | Ku : ");
        stream.print(
            autoTune.getResult().ultimateGain,
            3);
        stream.print(" | Tu : ");
        stream.print(
            autoTune.getResult()
                .ultimatePeriodSeconds,
            1);
        stream.print('s');
    }
    else if (tuneStatus ==
             AutoTuneStatus::Running)
    {
        stream.print(" | Cycle : ");
        stream.print(
            autoTune.getCompletedCycles());
        stream.print('/');
        stream.print(
            autoTuneSettings.cycles);
    }

    stream.println(' ');
}
