#include <Regulator/PIDAutoTune.h>

namespace
{
    constexpr double_t PI_VALUE =
        3.14159265358979323846;

    constexpr uint32_t TIMEOUT_MAX_SECONDS =
        86400;

    constexpr uint32_t MINIMUM_CYCLE_MAX_SECONDS =
        3600;
}

void PIDAutoTune::reset()
{
    activeSettings = Settings{};
    activeSetpoint = 0.0;
    activeDirection =
        ProcessDirection::OutputRaisesInput;

    status = Status::Idle;
    error = Error::None;
    result = Result{};

    startedAt = 0;
    commandValid = false;
    command = 0.0;

    clearMeasurements();
}

bool PIDAutoTune::start(
    uint32_t now,
    const Settings& settings,
    double_t setpoint,
    double_t outputMinimum,
    double_t outputMaximum,
    ProcessDirection direction)
{
    reset();

    const bool directionIsValid =
        direction ==
            ProcessDirection::OutputRaisesInput ||
        direction ==
            ProcessDirection::OutputLowersInput;

    if (!directionIsValid ||
        !settingsAreValid(
            settings,
            setpoint,
            outputMinimum,
            outputMaximum))
    {
        fail(Error::InvalidSettings);
        return false;
    }

    activeSettings = settings;
    activeDirection = direction;
    activeSetpoint = normalizeInput(setpoint);
    startedAt = now;
    status = Status::WaitingForMeasurement;

    return true;
}

void PIDAutoTune::cancel(Error cancellationError)
{
    if (!isActive())
        return;

    status = Status::Cancelled;
    error = cancellationError;
    commandValid = false;
    command = 0.0;
}

void PIDAutoTune::rejectTunings()
{
    if (status != Status::Succeeded)
        return;

    fail(Error::TuningsOutOfRange);
}

void PIDAutoTune::update(
    uint32_t now,
    bool measurementValid,
    double_t processValue)
{
    if (!isActive())
        return;

    const uint32_t timeoutMs =
        activeSettings.timeoutSeconds *
        1000UL;

    if ((now - startedAt) >= timeoutMs)
    {
        fail(
            candidateOscillationSeen
                ? Error::InsufficientOscillation
                : Error::Timeout);
        return;
    }

    const bool validValue =
        measurementValid &&
        std::isfinite(processValue);

    if (!validValue)
    {
        if (status == Status::Running)
            fail(Error::InvalidMeasurement);

        return;
    }

    if (processValue < activeSettings.inputMin ||
        processValue > activeSettings.inputMax)
    {
        fail(Error::InputOutOfRange);
        return;
    }

    /*
     * La machine d'oscillation travaille toujours comme si une sortie plus
     * forte faisait monter l'entrée. Les contrôles de sécurité ci-dessus
     * restent volontairement exprimés dans l'unité physique.
     */
    const double_t normalizedInput =
        normalizeInput(processValue);

    if (status == Status::WaitingForMeasurement)
    {
        beginOscillation(
            now,
            normalizedInput);
        return;
    }

    updateOscillation(
        now,
        normalizedInput);
}

double_t PIDAutoTune::normalizeInput(
    double_t processValue) const
{
    return
        activeDirection ==
                ProcessDirection::OutputRaisesInput
            ? processValue
            : -processValue;
}

bool PIDAutoTune::isActive() const
{
    return
        status == Status::WaitingForMeasurement ||
        status == Status::Running;
}

bool PIDAutoTune::hasCommand() const
{
    return commandValid;
}

double_t PIDAutoTune::readCommand() const
{
    return command;
}

PIDAutoTune::Status PIDAutoTune::getStatus() const
{
    return status;
}

PIDAutoTune::Error PIDAutoTune::getError() const
{
    return error;
}

uint8_t PIDAutoTune::getCompletedCycles() const
{
    const uint8_t completedPeaks =
        highPeakCount < lowPeakCount
            ? highPeakCount
            : lowPeakCount;

    return
        completedPeaks > 0
            ? completedPeaks - 1
            : 0;
}

const PIDAutoTune::Result&
PIDAutoTune::getResult() const
{
    return result;
}

bool PIDAutoTune::settingsAreValid(
    const Settings& settings,
    double_t setpoint,
    double_t outputMinimum,
    double_t outputMaximum)
{
    if (!std::isfinite(setpoint) ||
        !std::isfinite(outputMinimum) ||
        !std::isfinite(outputMaximum) ||
        outputMinimum < 0.0 ||
        outputMaximum > 1.0 ||
        outputMinimum > outputMaximum)
    {
        return false;
    }

    if (!std::isfinite(settings.outputLow) ||
        !std::isfinite(settings.outputHigh) ||
        !std::isfinite(settings.noiseBand) ||
        !std::isfinite(settings.inputMin) ||
        !std::isfinite(settings.inputMax) ||
        !std::isfinite(
            settings.stabilityTolerance))
    {
        return false;
    }

    if (settings.outputLow < outputMinimum ||
        settings.outputHigh > outputMaximum ||
        settings.outputLow >= settings.outputHigh ||
        settings.noiseBand <= 0.0 ||
        settings.inputMin >= settings.inputMax)
    {
        return false;
    }

    if (setpoint - settings.noiseBand <=
            settings.inputMin ||
        setpoint + settings.noiseBand >=
            settings.inputMax)
    {
        return false;
    }

    return
        settings.timeoutSeconds > 0 &&
        settings.timeoutSeconds <=
            TIMEOUT_MAX_SECONDS &&
        settings.minimumCycleSeconds > 0 &&
        settings.minimumCycleSeconds <=
            MINIMUM_CYCLE_MAX_SECONDS &&
        settings.stabilityTolerance >= 0.05 &&
        settings.stabilityTolerance <= 0.50 &&
        settings.cycles >= 2 &&
        settings.cycles <= MAX_CYCLES &&
        settings.timeoutSeconds >
            settings.minimumCycleSeconds *
                (settings.cycles + 1UL);
}

void PIDAutoTune::clearMeasurements()
{
    candidateOscillationSeen = false;
    outputHigh = false;
    phaseExtreme = 0.0;
    phaseExtremeAt = 0;
    highPeakCount = 0;
    lowPeakCount = 0;

    for (uint8_t i = 0;
         i < MAX_PEAKS;
         i++)
    {
        highPeaks[i] = 0.0;
        highPeakTimes[i] = 0;
        lowPeaks[i] = 0.0;
        lowPeakTimes[i] = 0;
    }
}

void PIDAutoTune::beginOscillation(
    uint32_t now,
    double_t processValue)
{
    outputHigh =
        processValue <= activeSetpoint;

    phaseExtreme = processValue;
    phaseExtremeAt = now;

    status = Status::Running;
    commandValid = true;
    command =
        outputHigh
            ? activeSettings.outputHigh
            : activeSettings.outputLow;
}

void PIDAutoTune::updateOscillation(
    uint32_t now,
    double_t processValue)
{
    if (outputHigh)
    {
        if (processValue < phaseExtreme)
        {
            phaseExtreme = processValue;
            phaseExtremeAt = now;
        }
    }
    else if (processValue > phaseExtreme)
    {
        phaseExtreme = processValue;
        phaseExtremeAt = now;
    }

    const double_t upperThreshold =
        activeSetpoint +
        activeSettings.noiseBand;

    const double_t lowerThreshold =
        activeSetpoint -
        activeSettings.noiseBand;

    if (outputHigh &&
        processValue >= upperThreshold)
    {
        recordLowPeak();
        outputHigh = false;
        phaseExtreme = processValue;
        phaseExtremeAt = now;
    }
    else if (!outputHigh &&
             processValue <= lowerThreshold)
    {
        recordHighPeak();
        outputHigh = true;
        phaseExtreme = processValue;
        phaseExtremeAt = now;
    }

    const uint8_t requiredPeaks =
        activeSettings.cycles + 1;

    if (highPeakCount >= requiredPeaks &&
        lowPeakCount >= requiredPeaks)
    {
        candidateOscillationSeen = true;

        if (tryFinish())
            return;
    }

    commandValid = true;
    command =
        outputHigh
            ? activeSettings.outputHigh
            : activeSettings.outputLow;
}

void PIDAutoTune::recordHighPeak()
{
    appendPeak(
        phaseExtreme,
        phaseExtremeAt,
        highPeaks,
        highPeakTimes,
        highPeakCount);
}

void PIDAutoTune::recordLowPeak()
{
    appendPeak(
        phaseExtreme,
        phaseExtremeAt,
        lowPeaks,
        lowPeakTimes,
        lowPeakCount);
}

void PIDAutoTune::appendPeak(
    double_t value,
    uint32_t time,
    double_t* values,
    uint32_t* times,
    uint8_t& count)
{
    const uint8_t capacity =
        activeSettings.cycles + 1;

    if (count >= capacity)
    {
        for (uint8_t i = 1;
             i < capacity;
             i++)
        {
            values[i - 1] = values[i];
            times[i - 1] = times[i];
        }

        count--;
    }

    values[count] = value;
    times[count] = time;
    count++;
}

bool PIDAutoTune::tryFinish()
{
    const uint8_t cycles =
        activeSettings.cycles;

    double_t highPeakSum = 0.0;
    double_t lowPeakSum = 0.0;
    double_t periodMsSum = 0.0;

    for (uint8_t i = 1;
         i <= cycles;
         i++)
    {
        highPeakSum += highPeaks[i];
        lowPeakSum += lowPeaks[i];

        periodMsSum +=
            static_cast<uint32_t>(
                highPeakTimes[i] -
                highPeakTimes[i - 1]);

        periodMsSum +=
            static_cast<uint32_t>(
                lowPeakTimes[i] -
                lowPeakTimes[i - 1]);
    }

    const double_t averageHighPeak =
        highPeakSum / cycles;

    const double_t averageLowPeak =
        lowPeakSum / cycles;

    const double_t averagePeriodMs =
        periodMsSum /
        (2.0 * cycles);

    if (!peaksAreStable(
            averageHighPeak,
            averageLowPeak,
            averagePeriodMs))
    {
        return false;
    }

    const double_t oscillationAmplitude =
        (averageHighPeak -
         averageLowPeak) /
        2.0;

    const double_t amplitudeSquared =
        oscillationAmplitude *
            oscillationAmplitude -
        activeSettings.noiseBand *
            activeSettings.noiseBand;

    if (!std::isfinite(amplitudeSquared) ||
        amplitudeSquared <= 0.0 ||
        !std::isfinite(averagePeriodMs) ||
        averagePeriodMs <= 0.0)
    {
        return false;
    }

    const double_t effectiveAmplitude =
        std::sqrt(amplitudeSquared);

    const double_t relayAmplitude =
        (activeSettings.outputHigh -
         activeSettings.outputLow) /
        2.0;

    result.ultimateGain =
        4.0 * relayAmplitude /
        (PI_VALUE * effectiveAmplitude);

    result.ultimatePeriodSeconds =
        averagePeriodMs / 1000.0;

    /* Forme parallèle du PID Ziegler-Nichols classique. */
    result.kp =
        0.6 * result.ultimateGain;

    result.ki =
        1.2 * result.ultimateGain /
        result.ultimatePeriodSeconds;

    result.kd =
        0.075 * result.ultimateGain *
        result.ultimatePeriodSeconds;

    if (!std::isfinite(result.ultimateGain) ||
        !std::isfinite(result.kp) ||
        !std::isfinite(result.ki) ||
        !std::isfinite(result.kd) ||
        result.ultimateGain <= 0.0 ||
        result.kp < 0.0 ||
        result.ki < 0.0 ||
        result.kd < 0.0)
    {
        return false;
    }

    status = Status::Succeeded;
    error = Error::None;
    commandValid = false;
    command = 0.0;

    return true;
}

bool PIDAutoTune::peaksAreStable(
    double_t averageHighPeak,
    double_t averageLowPeak,
    double_t averagePeriodMs) const
{
    const double_t amplitude =
        (averageHighPeak -
         averageLowPeak) /
        2.0;

    const double_t minimumPeriodMs =
        activeSettings.minimumCycleSeconds *
        1000.0;

    if (!std::isfinite(amplitude) ||
        amplitude <= activeSettings.noiseBand ||
        !std::isfinite(averagePeriodMs) ||
        averagePeriodMs < minimumPeriodMs)
    {
        return false;
    }

    const double_t allowedPeakVariation =
        amplitude *
        activeSettings.stabilityTolerance;

    const double_t allowedPeriodVariation =
        averagePeriodMs *
        activeSettings.stabilityTolerance;

    for (uint8_t i = 1;
         i <= activeSettings.cycles;
         i++)
    {
        if (std::fabs(
                highPeaks[i] -
                averageHighPeak) >
                allowedPeakVariation ||
            std::fabs(
                lowPeaks[i] -
                averageLowPeak) >
                allowedPeakVariation)
        {
            return false;
        }

        const double_t highPeriod =
            static_cast<uint32_t>(
                highPeakTimes[i] -
                highPeakTimes[i - 1]);

        const double_t lowPeriod =
            static_cast<uint32_t>(
                lowPeakTimes[i] -
                lowPeakTimes[i - 1]);

        if (std::fabs(
                highPeriod -
                averagePeriodMs) >
                allowedPeriodVariation ||
            std::fabs(
                lowPeriod -
                averagePeriodMs) >
                allowedPeriodVariation)
        {
            return false;
        }
    }

    return true;
}

void PIDAutoTune::fail(Error failure)
{
    status = Status::Failed;
    error = failure;
    result = Result{};
    commandValid = false;
    command = 0.0;
}
