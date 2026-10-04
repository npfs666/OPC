#include <Regulator/PID.h>

#include <Arduino.h>
#include <Measurements/Measurement.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    // Kp : fraction de sortie par unité de mesure. Ti et Td en secondes.
    constexpr double_t KP_MIN = 0.01;
    constexpr double_t KP_MAX = 100.0;
    constexpr double_t TI_MAX = 100000.0;
    constexpr double_t TD_MAX = 10000.0;

    constexpr double_t DEFAULT_SETPOINT_MIN = -50.0;
    constexpr double_t DEFAULT_SETPOINT_MAX = 250.0;

    constexpr ParameterOption PID_OPERATION_OPTIONS[] = {
        {static_cast<int32_t>(PID::Operation::Auto), "Auto"},
        {static_cast<int32_t>(PID::Operation::Manual), "Manuel"}
    };

    constexpr ParameterOption AUTOTUNE_RULE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                PID::AutoTuneRule::TyreusLuyben),
            "Tyreus-Luyben"
        },
        {
            static_cast<int32_t>(
                PID::AutoTuneRule::NoOvershoot),
            "Sans dépass."
        },
        {
            static_cast<int32_t>(
                PID::AutoTuneRule::SomeOvershoot),
            "Peu dépass."
        },
        {
            static_cast<int32_t>(
                PID::AutoTuneRule::ZieglerNichols),
            "Z-N classique"
        }
    };

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
        double_t ti,
        double_t td)
    {
        return
            std::isfinite(kp) &&
            std::isfinite(ti) &&
            std::isfinite(td) &&
            kp >= KP_MIN && kp <= KP_MAX &&
            ti >= 0.0 && ti <= TI_MAX &&
            td >= 0.0 && td <= TD_MAX;
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

    bool readSelectionDraft(
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
                Parameter::Type::Selection)
        {
            return false;
        }

        value = draft->selectionValue;

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
    setpointRamp.begin();
    scheduledSetpoint.begin();
    setpointMinimum = DEFAULT_SETPOINT_MIN;
    setpointMaximum = DEFAULT_SETPOINT_MAX;
    autoTuneParametersRegistered = false;
    autoTuneTuningsApplied = false;
    autoTuneOwnerKey = nullptr;

    reset();
}

void PID::resetController()
{
    manualHandover = false;
    integralTerm = 0.0;
    integralMode = settings.mode;
    holdController();
}

void PID::holdController()
{
    freezeController();
    invalidateCommand();
}

void PID::freezeController()
{
    filteredDerivative = 0.0;
    previousMeasurement = 0.0;
    previousTime = 0;
    initialized = false;
}

bool PID::setSetpointLimits(
    double_t minimum,
    double_t maximum)
{
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        minimum >= maximum)
    {
        return false;
    }

    setpointMinimum = minimum;
    setpointMaximum = maximum;

    return true;
}

void PID::setSchedule(
    const TimeSchedule& schedule,
    double_t reducedSetpoint)
{
    scheduledSetpoint.attach(schedule, reducedSetpoint);
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
    setpointRamp.restart();
    resetController();
}

void PID::stop()
{
    if (autoTune.isActive())
        autoTune.cancel();

    settings.enabled = false;
    setpointRamp.restart();
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
    double_t ti,
    double_t td)
{
    if (autoTune.isActive() ||
        !tuningsAreSupported(
            kp,
            ti,
            td))
    {
        return false;
    }

    settings.kp = kp;
    settings.ti = ti;
    settings.td = td;

    autoTune.reset();

    // Intégrale conservée : le changement de gains se fait sans à-coup.
    holdController();

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

    // L'intégrale sera ramenée dans les nouvelles limites au prochain calcul.
    holdController();

    return true;
}

bool PID::startAutoTune(uint32_t now)
{
    // L'opérateur a la main : pas d'essai en manuel.
    if (settings.operation == Operation::Manual)
        return false;

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
            settings.ti,
            settings.td) &&
        std::isfinite(settings.outputMin) &&
        std::isfinite(settings.outputMax) &&
        settings.outputMin >= 0.0 &&
        settings.outputMax <= 1.0 &&
        settings.outputMin <=
            settings.outputMax;
}

void PID::readOutputLimits(
    double_t& minimum,
    double_t& maximum) const
{
    minimum = settings.outputMin;
    maximum = settings.outputMax;
}

int8_t PID::actionDirection() const
{
    return settings.mode == Mode::Cooling ? -1 : 1;
}

bool PID::isAutomatic() const
{
    return settings.operation == Operation::Auto &&
           settings.enabled &&
           !autoTune.isActive();
}

double_t PID::integralTime() const
{
    return settings.ti;
}

bool PID::readSetpoint(double_t& setpoint) const
{
    if (!settings.enabled ||
        autoTune.isActive() ||
        !setpointRamp.hasActiveSetpoint())
    {
        return false;
    }

    setpoint = setpointRamp.activeSetpoint();
    return true;
}

void PID::update(uint32_t now)
{
    if (settings.operation == Operation::Manual)
    {
        updateManual();
        return;
    }

    updateControl(now);

    // Suivi : un passage en manuel part de la sortie du moment.
    if (isCommandValid())
        settings.manualOutput = readCommand() * 100.0;
}

void PID::updateManual()
{
    // L'opérateur prend la main : un essai en cours est abandonné.
    if (autoTune.isActive())
    {
        autoTune.cancel();
        settings.enabled = false;
    }

    // La rampe repartira de la mesure au retour en automatique.
    setpointRamp.restart();
    freezeController();
    manualHandover = true;

    // Ni mesure ni repli : la sortie manuelle s'applique telle quelle.
    handoverCommand =
        constrain(settings.manualOutput, 0.0, 100.0) / 100.0;
    writeCommand(handoverCommand);
}

void PID::updateControl(uint32_t now)
{
    /*
     * Verrouillé par une alarme (boucle ouverte) : sorties en sécurité,
     * intégrale figée jusqu'à l'acquittement.
     */
    if (isInterlocked())
    {
        setpointRamp.resume(now);
        freezeController();
        return;
    }

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
        setpointRamp.resume(now);

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
                    result.ti,
                    result.td))
            {
                settings.kp = result.kp;
                settings.ti = result.ti;
                settings.td = result.td;
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
        // Une réactivation (menu ou start()) repart d'une intégrale nulle.
        setpointRamp.restart();
        resetController();
        return;
    }

    double_t target = settings.setpoint;

    // Arrêt programmé : l'intégrale repart de zéro à la plage suivante.
    if (!scheduledSetpoint.update(
            settings.setpoint,
            target))
    {
        setpointRamp.restart();
        resetController();
        return;
    }

    if (!controlSettingsAreValid())
    {
        setpointRamp.resume(now);
        resetController();
        return;
    }

    /*
     * Mesure invalide : sortie en sécurité ou maintenue selon le réglage de
     * repli, intégrale gardée pour une reprise sans à-coup.
     */
    if (!measurementValid)
    {
        // Après un défaut, la sortie manuelle n'est plus une référence.
        manualHandover = false;
        setpointRamp.resume(now);
        handleMeasurementFault(
            now,
            measurement != nullptr
                ? measurement->getStatus()
                : MeasurementStatus::Invalid);
        freezeController();
        return;
    }

    if (!setpointRamp.update(
            now,
            target,
            processValue))
    {
        resetController();
        return;
    }

    updateAutomatic(
        now,
        processValue,
        setpointRamp.activeSetpoint());
}

void PID::updateAutomatic(
    uint32_t now,
    double_t processValue,
    double_t activeSetpoint)
{
    // Une intégrale accumulée dans l'autre sens d'action n'a plus de sens.
    if (settings.ti <= 0.0 ||
        integralMode != settings.mode)
    {
        integralTerm = 0.0;
        integralMode = settings.mode;
    }

    if (!initialized)
    {
        previousMeasurement = processValue;
        previousTime = now;
        filteredDerivative = 0.0;
        initialized = true;

        /*
         * Retour de manuel : l'intégrale reprend la sortie manuelle, qui
         * couvre ce premier cycle. La dérivée repart de zéro.
         */
        if (manualHandover)
        {
            manualHandover = false;

            const double_t actionSign =
                settings.mode == Mode::Heating ? 1.0 : -1.0;

            const double_t proportionalTerm =
                settings.kp *
                actionSign *
                (activeSetpoint - processValue);

            integralTerm =
                settings.ti > 0.0
                    ? constrain(
                          handoverCommand - proportionalTerm,
                          settings.outputMin,
                          settings.outputMax)
                    : 0.0;

            integralMode = settings.mode;
            writeCommand(handoverCommand);
            return;
        }

        // Fin d'un maintien : la commande maintenue couvre ce premier cycle,
        // sans repasser par l'état sûr.
        if (!isInFallback())
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
        (activeSetpoint -
         processValue);

    // Dérivée sur la mesure : un changement de consigne ne crée pas de pic.
    const double_t rawDerivative =
        actionSign *
        (-(processValue -
           previousMeasurement) /
         dt);

    if (settings.td > 0.0)
    {
        // Filtre du premier ordre : limite l'effet du bruit de mesure.
        const double_t filterTime =
            settings.td /
            DERIVATIVE_FILTER_RATIO;

        filteredDerivative +=
            (dt / (filterTime + dt)) *
            (rawDerivative - filteredDerivative);
    }
    else
    {
        filteredDerivative = 0.0;
    }

    const double_t proportionalTerm =
        settings.kp * error;

    const double_t derivativeTerm =
        settings.kp *
        settings.td *
        filteredDerivative;

    if (!std::isfinite(proportionalTerm) ||
        !std::isfinite(derivativeTerm))
    {
        resetController();
        return;
    }

    if (settings.ti > 0.0)
    {
        const double_t candidateIntegral =
            integralTerm +
            settings.kp * error * dt /
                settings.ti;

        const double_t candidateOutput =
            proportionalTerm +
            candidateIntegral +
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
            std::isfinite(candidateOutput))
        {
            if (outputInsideLimits ||
                unwindsHighSaturation ||
                unwindsLowSaturation)
            {
                integralTerm = candidateIntegral;
            }
            else if (candidateOutput > settings.outputMax)
            {
                /*
                 * Anti-windup : l'intégrale monte juste assez pour que la
                 * commande atteigne la limite, sans jamais la dépasser.
                 * Sans cela, la sortie pourrait rester bloquée sous la
                 * saturation alors que l'erreur persiste.
                 */
                const double_t atLimit =
                    settings.outputMax -
                    proportionalTerm -
                    derivativeTerm;

                if (atLimit > integralTerm)
                    integralTerm = atLimit;
            }
            else
            {
                const double_t atLimit =
                    settings.outputMin -
                    proportionalTerm -
                    derivativeTerm;

                if (atLimit < integralTerm)
                    integralTerm = atLimit;
            }
        }

        // L'intégrale seule ne dépasse jamais la plage de sortie, même
        // après une réduction des limites ou une reprise.
        integralTerm = constrain(
            integralTerm,
            settings.outputMin,
            settings.outputMax);
    }

    double_t output =
        proportionalTerm +
        integralTerm +
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
    setpointRamp.resume(now);

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

    /*
     * Reprise sans à-coup après une application de réglages : l'intégrale
     * reflète la charge du process et redonne tout de suite la bonne
     * commande. Un PID arrêté la remettra à zéro à sa prochaine mise à jour.
     */
    holdController();
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

    // Mode manuel en tête du menu, non sauvegardé (retour en Auto au
    // démarrage).
    auto operation = list.forOwner({
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName(),
        false
    });

    operation.addSelection(
        "operation",
        "Commande",
        settings.operation,
        PID_OPERATION_OPTIONS);

    operation.addDouble(
        "manual_output",
        "Sortie man.",
        settings.manualOutput,
        0.0,
        100.0,
        1.0,
        1,
        "%",
        false,
        0.1);

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
        setpointMinimum,
        setpointMaximum,
        0.1,
        1,
        inputUnit);

    scheduledSetpoint.registerParameters(
        parameters,
        setpointMinimum,
        setpointMaximum,
        0.1,
        1,
        inputUnit);

    parameters.addDouble(
        "kp",
        "Kp",
        settings.kp,
        KP_MIN,
        KP_MAX,
        0.01,
        2,
        "1/°C");

    // Premier pas de 10 s, puis réglage fin à la seconde.
    parameters.addDouble(
        "ti",
        "Ti",
        settings.ti,
        0.0,
        TI_MAX,
        10.0,
        0,
        "s",
        false,
        1.0);

    parameters.addDouble(
        "td",
        "Td",
        settings.td,
        0.0,
        TD_MAX,
        1.0,
        1,
        "s",
        false,
        0.1);

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

    registerFaultParameters(list);

    // Réglages de conduite : appliqués sans arrêter la régulation.
    list.setLive(getConfigurationKey(), "operation");
    list.setLive(getConfigurationKey(), "manual_output");
    list.setLive(getConfigurationKey(), "setpoint");
    list.setLive(getConfigurationKey(), "reduced_setpoint");
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
            1) &&
        parameters.addSelection(
            "autotune_rule",
            "Règle",
            autoTuneSettings.rule,
            AUTOTUNE_RULE_OPTIONS);

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
    int32_t rule = 0;

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
            cycles) ||
        !readSelectionDraft(
            editor,
            autoTuneOwnerKey,
            "autotune_rule",
            rule))
    {
        return false;
    }

    if (timeoutSeconds < 0 ||
        minimumCycleSeconds < 0 ||
        cycles < 0 ||
        cycles > UINT8_MAX ||
        rule < 0 ||
        rule > UINT8_MAX)
    {
        return false;
    }

    tuneSettings.rule =
        static_cast<AutoTuneRule>(rule);

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
    stream.print(
        setpointRamp.hasActiveSetpoint()
            ? setpointRamp.activeSetpoint()
            : settings.setpoint,
        2);

    if (setpointRamp.settings.enabled)
    {
        stream.print(" | Target : ");
        stream.print(settings.setpoint, 2);
    }

    stream.print(" | Mode : ");
    stream.print(
        settings.mode == Mode::Heating
            ? "heating"
            : settings.mode == Mode::Cooling
                ? "cooling"
                : "invalid");

    const AutoTuneStatus tuneStatus =
        autoTune.getStatus();

    if (scheduledSetpoint.isAttached())
    {
        stream.print(" | Prog : ");
        stream.print(
            ScheduledSetpoint::stateName(
                scheduledSetpoint.state()));
    }

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
        stream.print(" | Kp : ");
        stream.print(autoTune.getResult().kp, 3);
        stream.print(" | Ti : ");
        stream.print(autoTune.getResult().ti, 0);
        stream.print("s | Td : ");
        stream.print(autoTune.getResult().td, 1);
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
