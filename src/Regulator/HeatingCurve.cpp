// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/HeatingCurve.h>

#include <Arduino.h>
#include <Measurements/Measurement.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    // Reprise du chauffage sous Arrêt été moins cet écart.
    constexpr double_t SUMMER_HYSTERESIS = 1.0;

    // Écart minimal entre les points froid et doux.
    constexpr double_t MIN_OUTDOOR_SPAN = 5.0;

    bool readNumberDraft(
        const ParameterEditor& editor,
        const char* ownerKey,
        const char* key,
        double_t& value)
    {
        const ParameterDraft* draft = editor.find(ownerKey, key);

        if (draft == nullptr ||
            draft->parameter == nullptr ||
            draft->parameter->type != Parameter::Type::Double)
        {
            return false;
        }

        value = draft->numberValue;
        return std::isfinite(value);
    }

    bool curveIsConsistent(
        double_t coldOutdoor,
        double_t coldFlow,
        double_t mildOutdoor,
        double_t mildFlow,
        double_t minFlow,
        double_t maxFlow)
    {
        return
            mildOutdoor - coldOutdoor >= MIN_OUTDOOR_SPAN &&
            coldFlow >= mildFlow &&
            minFlow < maxFlow;
    }
}

void HeatingCurve::begin(
    const char* key,
    const char* name,
    Measurement& outdoor)
{
    Regulator::begin(key, name);

    this->outdoor = &outdoor;

    settings = Settings{};
    scheduledSetpoint.begin();

    flowSetpoint = 0.0;
    filtered = 0.0;
    filterStarted = false;
    usedOutdoor = 0.0;
    fallback = false;
    summer = false;
    currentState = State::Waiting;
}

void HeatingCurve::setSchedule(
    const TimeSchedule& schedule,
    double_t reducedRoomSetpoint)
{
    scheduledSetpoint.attach(schedule, reducedRoomSetpoint);
}

bool HeatingCurve::requiresClock() const
{
    return scheduledSetpoint.isAttached();
}

double_t HeatingCurve::flowFor(
    double_t outdoorValue,
    double_t room) const
{
    if (!curveIsConsistent(
            settings.coldOutdoor,
            settings.coldFlow,
            settings.mildOutdoor,
            settings.mildFlow,
            settings.minFlow,
            settings.maxFlow))
    {
        return settings.minFlow;
    }

    const double_t slope =
        (settings.coldFlow - settings.mildFlow) /
        (settings.mildOutdoor - settings.coldOutdoor);

    // Modèle linéaire du bâtiment : départ - ambiance = pente × (ambiance -
    // extérieur). Une ambiance plus haute décale la courbe de (1 + pente).
    const double_t flow =
        settings.mildFlow +
        slope * (settings.mildOutdoor - outdoorValue) +
        (1.0 + slope) * (room - REFERENCE_ROOM);

    return constrain(flow, settings.minFlow, settings.maxFlow);
}

bool HeatingCurve::readOutdoor(
    uint32_t now,
    double_t& measured)
{
    if (outdoor == nullptr ||
        !outdoor->isValid() ||
        !std::isfinite(outdoor->getValue()))
    {
        return false;
    }

    measured = outdoor->getValue();

    const double_t tauSeconds = settings.buildingTimeConstant * 3600.0;

    if (!filterStarted || tauSeconds <= 0.0)
    {
        filtered = measured;
        filterStarted = true;
    }
    else
    {
        const double_t elapsed = (now - filterTime) / 1000.0;
        filtered += (measured - filtered) * elapsed / (tauSeconds + elapsed);
    }

    filterTime = now;
    return true;
}

void HeatingCurve::update(uint32_t now)
{
    double_t measured = 0.0;
    const bool measurementValid = readOutdoor(now, measured);

    // Pas encore de mesure (démarrage, reprise de l'acquisition) : la
    // dernière valeur filtrée, ou l'état sûr au démarrage.
    const bool notReady =
        !measurementValid &&
        (outdoor == nullptr ||
         outdoor->getStatus() == MeasurementStatus::NotReady);

    if (notReady && !filterStarted)
    {
        currentState = State::Waiting;
        invalidateCommand();
        return;
    }

    fallback = !measurementValid && !notReady;

    if (measurementValid)
        usedOutdoor = filtered;
    else if (notReady)
        usedOutdoor = measured = filtered;
    else
        usedOutdoor = measured = settings.fallbackOutdoor;

    if (usedOutdoor >= settings.summerLimit)
        summer = true;
    else if (usedOutdoor <= settings.summerLimit - SUMMER_HYSTERESIS)
        summer = false;

    double_t room = settings.roomSetpoint;
    const bool scheduled = scheduledSetpoint.update(settings.roomSetpoint, room);

    // Heure inconnue avec un programme : consigne indéterminée, état sûr.
    if (scheduledSetpoint.state() == ScheduledSetpoint::State::ClockInvalid)
    {
        currentState = State::ClockInvalid;
        invalidateCommand();
        return;
    }

    if (!scheduled || summer)
    {
        // Hors-gel sur la mesure du moment, sans attendre le filtre.
        if (measured < settings.frostLimit)
        {
            currentState = State::Frost;
            flowSetpoint = settings.minFlow;
            writeCommand(1.0);
            return;
        }

        currentState = summer ? State::Summer : State::Off;
        writeCommand(0.0);
        return;
    }

    currentState =
        scheduledSetpoint.state() == ScheduledSetpoint::State::Reduced
            ? State::Reduced
            : State::Comfort;

    flowSetpoint = flowFor(usedOutdoor, room);
    writeCommand(1.0);
}

bool HeatingCurve::readSetpoint(double_t& setpoint) const
{
    // Inhibée par la glue : commande 0, pas de consigne.
    if (!isCommandValid() || readCommand() < 0.5)
        return false;

    setpoint = flowSetpoint;
    return true;
}

double_t HeatingCurve::outdoorTemperature() const
{
    return usedOutdoor;
}

bool HeatingCurve::isOutdoorFallback() const
{
    return fallback;
}

HeatingCurve::State HeatingCurve::state() const
{
    return currentState;
}

const char* HeatingCurve::stateName(State state)
{
    switch (state)
    {
    case State::Comfort:
        return "Confort";

    case State::Reduced:
        return "Réduit";

    case State::Summer:
        return "Été";

    case State::Off:
        return "Arrêt";

    case State::Frost:
        return "Hors-gel";

    case State::ClockInvalid:
        return "Heure invalide";

    default:
        return "Attente";
    }
}

double_t HeatingCurve::printValue() const
{
    double_t setpoint = 0.0;
    return readSetpoint(setpoint) ? setpoint : 0.0;
}

const char* HeatingCurve::getUnit() const
{
    return "°C";
}

void HeatingCurve::print(Stream& stream) const
{
    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");
    stream.print(stateName(currentState));
    stream.print(" | Ext : ");
    stream.print(usedOutdoor, 1);

    if (fallback)
        stream.print(" (secours)");

    double_t setpoint = 0.0;

    if (readSetpoint(setpoint))
    {
        stream.print(" | Depart : ");
        stream.print(setpoint, 1);
    }

    stream.println(' ');
}

void HeatingCurve::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    });

    parameters.addDouble(
        "room_setpoint", "Cons. ambiance", settings.roomSetpoint,
        5.0, 30.0, 0.5, 1, "°C");

    scheduledSetpoint.registerParameters(
        parameters, 5.0, 30.0, 0.5, 1, "°C");

    parameters.addDouble(
        "cold_outdoor", "T. ext. froid", settings.coldOutdoor,
        -30.0, 5.0, 1.0, 0, "°C");

    parameters.addDouble(
        "cold_flow", "Départ à froid", settings.coldFlow,
        20.0, 90.0, 1.0, 0, "°C");

    parameters.addDouble(
        "mild_outdoor", "T. ext. doux", settings.mildOutdoor,
        0.0, 25.0, 1.0, 0, "°C");

    parameters.addDouble(
        "mild_flow", "Départ à doux", settings.mildFlow,
        10.0, 60.0, 1.0, 0, "°C");

    parameters.addDouble(
        "min_flow", "Départ mini", settings.minFlow,
        10.0, 60.0, 1.0, 0, "°C");

    parameters.addDouble(
        "max_flow", "Départ maxi", settings.maxFlow,
        20.0, 90.0, 1.0, 0, "°C");

    parameters.addDouble(
        "summer_limit", "Arrêt été", settings.summerLimit,
        10.0, 25.0, 0.5, 1, "°C");

    parameters.addDouble(
        "frost_limit", "Hors-gel", settings.frostLimit,
        -10.0, 10.0, 0.5, 1, "°C");

    parameters.addDouble(
        "fallback_outdoor", "T. ext. secours", settings.fallbackOutdoor,
        -20.0, 15.0, 1.0, 0, "°C");

    parameters.addInteger(
        "building_tau", "Inertie bât.", settings.buildingTimeConstant,
        0, 48, 1, "h");

    // Réglages de conduite : appliqués sans arrêter la régulation.
    list.setLive(getConfigurationKey(), "room_setpoint");
    list.setLive(getConfigurationKey(), "reduced_setpoint");
}

bool HeatingCurve::validateParameters(
    const ParameterEditor& editor) const
{
    const char* key = getConfigurationKey();

    double_t coldOutdoor = 0.0;
    double_t coldFlow = 0.0;
    double_t mildOutdoor = 0.0;
    double_t mildFlow = 0.0;
    double_t minFlow = 0.0;
    double_t maxFlow = 0.0;

    if (!readNumberDraft(editor, key, "cold_outdoor", coldOutdoor) ||
        !readNumberDraft(editor, key, "cold_flow", coldFlow) ||
        !readNumberDraft(editor, key, "mild_outdoor", mildOutdoor) ||
        !readNumberDraft(editor, key, "mild_flow", mildFlow) ||
        !readNumberDraft(editor, key, "min_flow", minFlow) ||
        !readNumberDraft(editor, key, "max_flow", maxFlow))
    {
        return false;
    }

    return curveIsConsistent(
        coldOutdoor, coldFlow, mildOutdoor, mildFlow, minFlow, maxFlow);
}
