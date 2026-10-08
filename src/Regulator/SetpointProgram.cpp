// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/SetpointProgram.h>

#include <Arduino.h>
#include <Measurements/Measurement.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr uint32_t MS_PER_MINUTE = 60000UL;
    constexpr double_t MS_PER_HOUR = 3600000.0;

    // Départ différé et palier maxi, en minutes.
    constexpr int32_t MAX_DELAY_MINUTES = 1440;
    constexpr int32_t MAX_SOAK_MINUTES = 1440;

    constexpr double_t MAX_HOLDBACK = 100.0;

    // Reste d'arrondi des pas de rampe : la cible est atteinte.
    constexpr double_t RAMP_EPSILON = 1e-9;

    constexpr ParameterOption END_OPTIONS[] = {
        {static_cast<int32_t>(SetpointProgram::End::Stop), "Arrêt"},
        {static_cast<int32_t>(SetpointProgram::End::Hold), "Maintien"}
    };

    // Pointés par ParameterList : doivent rester valides.
    constexpr const char* PROGRAM_NAMES[] = {
        "Programme 1",
        "Programme 2",
        "Programme 3",
        "Programme 4"
    };

    constexpr const char* SEGMENT_NAMES[] = {
        "Segment 1",
        "Segment 2",
        "Segment 3",
        "Segment 4",
        "Segment 5",
        "Segment 6",
        "Segment 7",
        "Segment 8"
    };

    static_assert(
        sizeof(PROGRAM_NAMES) / sizeof(PROGRAM_NAMES[0]) ==
            SetpointProgram::MAX_PROGRAMS,
        "PROGRAM_NAMES doit contenir un nom par programme");

    static_assert(
        sizeof(SEGMENT_NAMES) / sizeof(SEGMENT_NAMES[0]) ==
            SetpointProgram::MAX_SEGMENTS,
        "SEGMENT_NAMES doit contenir un nom par segment");

    uint32_t addSaturated(uint32_t total, uint32_t elapsed)
    {
        return total > UINT32_MAX - elapsed ? UINT32_MAX : total + elapsed;
    }

    uint32_t remainingMs(uint32_t duration, uint32_t elapsed)
    {
        return elapsed < duration ? duration - elapsed : 0;
    }

    // Arrondi à la seconde supérieure : « 0 s » seulement quand c'est fini.
    uint32_t toSeconds(uint32_t ms)
    {
        return ms / 1000UL + (ms % 1000UL != 0 ? 1 : 0);
    }
}

void SetpointProgram::begin(
    const char* key,
    const char* name,
    Measurement& measurement,
    uint8_t programCount)
{
    Regulator::begin(key, name);

    this->measurement = &measurement;

    programs =
        programCount < 1 ? 1
        : programCount > MAX_PROGRAMS ? MAX_PROGRAMS
        : programCount;

    settings = Settings{};

    targetMinimum = 0.0;
    targetMaximum = 1300.0;
    rateMaximum = 999.0;
    unit = "°C";
    rateUnit = "°C/h";

    for (uint8_t p = 0; p < MAX_PROGRAMS; p++)
    {
        snprintf(
            programKeys[p],
            KEY_LENGTH,
            "%s.p%u",
            getConfigurationKey(),
            static_cast<unsigned>(p + 1));

        for (uint8_t s = 0; s < MAX_SEGMENTS; s++)
        {
            snprintf(
                segmentKeys[p][s],
                KEY_LENGTH,
                "%s.p%u.s%u",
                getConfigurationKey(),
                static_cast<unsigned>(p + 1),
                static_cast<unsigned>(s + 1));
        }
    }

    currentState = State::Idle;
    programIndex = 0;
    segmentIndex = 0;
    setpoint = 0.0;
    segmentStart = 0.0;
    heldBack = false;
    timeKnown = false;
    delayElapsedMs = 0;
    soakElapsedMs = 0;
    runElapsedMs = 0;
}

bool SetpointProgram::setLimits(
    double_t targetMinimum,
    double_t targetMaximum,
    double_t rateMaximum)
{
    if (!std::isfinite(targetMinimum) ||
        !std::isfinite(targetMaximum) ||
        !std::isfinite(rateMaximum) ||
        targetMinimum >= targetMaximum ||
        rateMaximum <= 0.0)
    {
        return false;
    }

    this->targetMinimum = targetMinimum;
    this->targetMaximum = targetMaximum;
    this->rateMaximum = rateMaximum;

    return true;
}

void SetpointProgram::setUnits(
    const char* unit,
    const char* rateUnit)
{
    if (unit != nullptr)
        this->unit = unit;

    if (rateUnit != nullptr)
        this->rateUnit = rateUnit;
}

uint8_t SetpointProgram::programCount() const
{
    return programs;
}

const SetpointProgram::Program& SetpointProgram::runningSettings() const
{
    return settings.programs[programIndex];
}

bool SetpointProgram::readMeasurement(double_t& value) const
{
    if (measurement == nullptr ||
        !measurement->isValid() ||
        !std::isfinite(measurement->getValue()))
    {
        return false;
    }

    value = measurement->getValue();
    return true;
}

bool SetpointProgram::outsideHoldback(
    double_t measured,
    double_t reference) const
{
    return settings.holdback > 0.0 &&
           std::fabs(measured - reference) > settings.holdback;
}

bool SetpointProgram::start()
{
    if (settings.selected < 1 || settings.selected > programs)
        return false;

    double_t measured = 0.0;

    if (!readMeasurement(measured))
        return false;

    programIndex = settings.selected - 1;
    segmentIndex = 0;
    setpoint = measured;
    segmentStart = measured;
    heldBack = false;
    delayElapsedMs = 0;
    soakElapsedMs = 0;
    runElapsedMs = 0;

    currentState =
        settings.startDelay > 0
            ? State::Delayed
            : State::Waiting;

    return true;
}

void SetpointProgram::stop()
{
    currentState = State::Idle;
    segmentIndex = 0;
    heldBack = false;
}

bool SetpointProgram::skipSegment()
{
    switch (currentState)
    {
    case State::Delayed:
        currentState = State::Waiting;
        return true;

    case State::Ramp:
    case State::Soak:
        nextSegment();
        return true;

    default:
        return false;
    }
}

void SetpointProgram::enterSegment(uint8_t index)
{
    segmentIndex = index;
    segmentStart = setpoint;
    soakElapsedMs = 0;
    heldBack = false;
    currentState = State::Ramp;
}

void SetpointProgram::nextSegment()
{
    if (segmentIndex + 1 < runningSettings().segmentCount)
        enterSegment(segmentIndex + 1);
    else
        finish();
}

void SetpointProgram::finish()
{
    const uint8_t count = runningSettings().segmentCount;

    if (count > 0 && segmentIndex >= count)
        segmentIndex = count - 1;

    heldBack = false;

    // Maintien : la consigne du moment, sans fin.
    currentState =
        settings.end == End::Hold
            ? State::Hold
            : State::Finished;
}

void SetpointProgram::updateRamp(
    uint32_t elapsedMs,
    double_t measured)
{
    const Segment& segment =
        runningSettings().segments[segmentIndex];

    const double_t target = segment.target;

    // Pleine puissance : consigne à la cible, palier quand la mesure y est
    // (à l'écart maxi près).
    if (!(segment.rate > 0.0))
    {
        setpoint = target;
        heldBack = false;

        const bool rising = target >= segmentStart;

        const bool reached =
            rising
                ? measured >= target - settings.holdback
                : measured <= target + settings.holdback;

        if (reached)
        {
            soakElapsedMs = 0;
            currentState = State::Soak;
        }

        return;
    }

    heldBack = outsideHoldback(measured, setpoint);

    if (heldBack)
        return;

    const double_t step = segment.rate * elapsedMs / MS_PER_HOUR;
    const double_t difference = target - setpoint;

    if (std::fabs(difference) <= step + RAMP_EPSILON)
    {
        setpoint = target;
        soakElapsedMs = 0;
        currentState = State::Soak;
        return;
    }

    setpoint += difference > 0.0 ? step : -step;
}

void SetpointProgram::updateSoak(
    uint32_t elapsedMs,
    double_t measured)
{
    const Segment& segment =
        runningSettings().segments[segmentIndex];

    // Cible modifiée au menu pendant le palier : prise en compte.
    setpoint = segment.target;

    heldBack = outsideHoldback(measured, setpoint);

    if (!heldBack)
        soakElapsedMs = addSaturated(soakElapsedMs, elapsedMs);

    if (soakElapsedMs >= segment.soak * MS_PER_MINUTE)
        nextSegment();
}

void SetpointProgram::update(uint32_t now)
{
    const uint32_t elapsedMs = timeKnown ? now - lastTime : 0;

    lastTime = now;
    timeKnown = true;

    // Inhibé par la glue : programme figé, commande 0 (voir Regulator).
    if (isInhibited())
    {
        writeCommand(hasSetpoint() ? 1.0 : 0.0);
        return;
    }

    switch (currentState)
    {
    case State::Idle:
    case State::Finished:
        writeCommand(0.0);
        return;

    case State::Delayed:
        delayElapsedMs = addSaturated(delayElapsedMs, elapsedMs);

        if (delayElapsedMs < settings.startDelay * MS_PER_MINUTE)
        {
            writeCommand(0.0);
            return;
        }

        currentState = State::Waiting;
        // Départ dès ce cycle si la mesure est valide.
        [[fallthrough]];

    case State::Waiting:
    {
        double_t measured = 0.0;

        if (!readMeasurement(measured))
        {
            writeCommand(0.0);
            return;
        }

        setpoint = measured;
        runElapsedMs = 0;
        enterSegment(0);

        // Pleine puissance : consigne à la cible dès ce cycle.
        updateRamp(0, measured);
        writeCommand(1.0);
        return;
    }

    default:
        break;
    }

    runElapsedMs = addSaturated(runElapsedMs, elapsedMs);

    // Segments retirés au menu pendant le programme : fin.
    if (currentState != State::Hold &&
        segmentIndex >= runningSettings().segmentCount)
    {
        finish();
    }

    double_t measured = 0.0;

    if ((currentState == State::Ramp || currentState == State::Soak) &&
        readMeasurement(measured))
    {
        if (currentState == State::Ramp)
            updateRamp(elapsedMs, measured);
        else
            updateSoak(elapsedMs, measured);
    }
    else
    {
        // Mesure invalide : programme figé, le suiveur applique sa règle
        // de défaut.
        heldBack = false;
    }

    writeCommand(currentState == State::Finished ? 0.0 : 1.0);
}

void SetpointProgram::resume(uint32_t now)
{
    Regulator::resume(now);

    // La pause ne compte pas dans le programme.
    lastTime = now;
}

bool SetpointProgram::readSetpoint(double_t& value) const
{
    if (isInhibited() || !hasSetpoint())
        return false;

    value = setpoint;
    return true;
}

bool SetpointProgram::hasSetpoint() const
{
    return currentState == State::Ramp ||
           currentState == State::Soak ||
           currentState == State::Hold;
}

bool SetpointProgram::isRunning() const
{
    return currentState != State::Idle &&
           currentState != State::Finished;
}

SetpointProgram::State SetpointProgram::state() const
{
    return currentState;
}

const char* SetpointProgram::stateName(State state)
{
    switch (state)
    {
    case State::Delayed:
        return "Départ différé";

    case State::Waiting:
        return "Attente mesure";

    case State::Ramp:
        return "Rampe";

    case State::Soak:
        return "Palier";

    case State::Hold:
        return "Maintien";

    case State::Finished:
        return "Terminé";

    default:
        return "Arrêt";
    }
}

uint8_t SetpointProgram::runningProgram() const
{
    return currentState != State::Idle ? programIndex + 1 : 0;
}

uint8_t SetpointProgram::segment() const
{
    switch (currentState)
    {
    case State::Ramp:
    case State::Soak:
    case State::Hold:
    case State::Finished:
        return segmentIndex + 1;

    default:
        return 0;
    }
}

uint8_t SetpointProgram::segmentCount() const
{
    return currentState != State::Idle
        ? runningSettings().segmentCount
        : 0;
}

bool SetpointProgram::readSegmentTarget(double_t& target) const
{
    if ((currentState != State::Ramp && currentState != State::Soak) ||
        segmentIndex >= runningSettings().segmentCount)
    {
        return false;
    }

    target = runningSettings().segments[segmentIndex].target;
    return true;
}

bool SetpointProgram::isHeldBack() const
{
    return heldBack;
}

bool SetpointProgram::isCooling() const
{
    double_t target = 0.0;

    return currentState == State::Ramp &&
           readSegmentTarget(target) &&
           target < segmentStart;
}

uint32_t SetpointProgram::elapsedSeconds() const
{
    return runElapsedMs / 1000UL;
}

uint32_t SetpointProgram::delayRemainingSeconds() const
{
    if (currentState != State::Delayed)
        return 0;

    return toSeconds(remainingMs(
        settings.startDelay * MS_PER_MINUTE,
        delayElapsedMs));
}

uint32_t SetpointProgram::soakRemainingSeconds() const
{
    if (currentState != State::Soak ||
        segmentIndex >= runningSettings().segmentCount)
    {
        return 0;
    }

    return toSeconds(remainingMs(
        runningSettings().segments[segmentIndex].soak * MS_PER_MINUTE,
        soakElapsedMs));
}

uint32_t SetpointProgram::remainingSeconds() const
{
    if (currentState != State::Ramp && currentState != State::Soak)
        return 0;

    const Program& program = runningSettings();

    double_t seconds = 0.0;
    double_t from = setpoint;

    for (uint8_t i = segmentIndex; i < program.segmentCount; i++)
    {
        const Segment& segment = program.segments[i];

        if (i == segmentIndex && currentState == State::Soak)
        {
            seconds += soakRemainingSeconds();
        }
        else
        {
            if (segment.rate > 0.0)
                seconds += std::fabs(segment.target - from) / segment.rate * 3600.0;

            seconds += segment.soak * 60.0;
        }

        from = segment.target;
    }

    return seconds >= UINT32_MAX
        ? UINT32_MAX
        : static_cast<uint32_t>(std::lround(seconds));
}

double_t SetpointProgram::printValue() const
{
    double_t value = 0.0;
    return readSetpoint(value) ? value : 0.0;
}

const char* SetpointProgram::getUnit() const
{
    return unit;
}

void SetpointProgram::print(Stream& stream) const
{
    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");
    stream.print(stateName(currentState));

    if (segment() > 0)
    {
        stream.print(" | P");
        stream.print(runningProgram());
        stream.print(" seg. ");
        stream.print(segment());
        stream.print('/');
        stream.print(segmentCount());
    }

    double_t value = 0.0;

    if (readSetpoint(value))
    {
        stream.print(" | Consigne : ");
        stream.print(value, 1);
    }

    if (heldBack)
        stream.print(" (attente)");

    stream.println(' ');
}

void SetpointProgram::registerParameters(ParameterList& list)
{
    const char* key = getConfigurationKey();

    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        key,
        getName()
    });

    parameters.addInteger(
        "program", "Programme", settings.selected,
        1, programs, 1);

    parameters.addInteger(
        "start_delay", "Départ diff.", settings.startDelay,
        0, MAX_DELAY_MINUTES, 15, "min");

    parameters.addDouble(
        "holdback", "Écart maxi", settings.holdback,
        0.0, MAX_HOLDBACK, 1.0, 0, unit);

    parameters.addSelection(
        "end", "Fin", settings.end, END_OPTIONS);

    // Réglages de conduite : un programme se modifie pendant la cuisson
    // sans couper les sorties.
    list.setLive(key, "program");
    list.setLive(key, "start_delay");
    list.setLive(key, "holdback");
    list.setLive(key, "end");

    for (uint8_t p = 0; p < programs; p++)
    {
        Program& program = settings.programs[p];

        ParameterOwner programOwner{
            "regulators",
            "Regulateur",
            programKeys[p],
            PROGRAM_NAMES[p]
        };

        programOwner.parentOwnerKey = key;

        list.forOwner(programOwner).addInteger(
            "segments", "Segments", program.segmentCount,
            1, MAX_SEGMENTS, 1);

        list.setLive(programKeys[p], "segments");

        for (uint8_t s = 0; s < MAX_SEGMENTS; s++)
        {
            Segment& segment = program.segments[s];

            // Valeurs par défaut hors des limites du template : ramenées.
            segment.target = constrain(
                segment.target, targetMinimum, targetMaximum);
            segment.rate = constrain(segment.rate, 0.0, rateMaximum);

            ParameterOwner segmentOwner{
                "regulators",
                "Regulateur",
                segmentKeys[p][s],
                SEGMENT_NAMES[s]
            };

            segmentOwner.parentOwnerKey = programKeys[p];

            auto segmentParameters = list.forOwner(segmentOwner);

            segmentParameters.addDouble(
                "rate", "Vitesse", segment.rate,
                0.0, rateMaximum, 10.0, 0, rateUnit, false, 1.0);

            segmentParameters.addDouble(
                "target", "Cible", segment.target,
                targetMinimum, targetMaximum, 10.0, 0, unit, false, 1.0);

            segmentParameters.addInteger(
                "soak", "Palier", segment.soak,
                0, MAX_SOAK_MINUTES, 5, "min");

            list.setLive(segmentKeys[p][s], "rate");
            list.setLive(segmentKeys[p][s], "target");
            list.setLive(segmentKeys[p][s], "soak");
        }
    }
}
