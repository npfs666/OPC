#include <Regulator/LimitAlarm.h>

#include <Measurements/Measurement.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    constexpr ParameterOption ABSOLUTE_TYPES[] = {
        {static_cast<int32_t>(LimitAlarm::Type::Max), "Max"},
        {static_cast<int32_t>(LimitAlarm::Type::Min), "Min"}
    };

    constexpr ParameterOption ALL_TYPES[] = {
        {static_cast<int32_t>(LimitAlarm::Type::Max), "Max"},
        {static_cast<int32_t>(LimitAlarm::Type::Min), "Min"},
        {static_cast<int32_t>(LimitAlarm::Type::DeviationHigh), "Écart haut"},
        {static_cast<int32_t>(LimitAlarm::Type::DeviationLow), "Écart bas"},
        {static_cast<int32_t>(LimitAlarm::Type::Band), "Hors bande"}
    };
}

LimitAlarm::LimitAlarm()
{
}

void LimitAlarm::begin(
    const char* key,
    const char* name,
    Measurement& measurement)
{
    Regulator::begin(key, name);
    beginAlarm();
    this->measurement = &measurement;
    reference = nullptr;
    started = false;
    restart();
}

void LimitAlarm::setReference(const Regulator& regulator)
{
    reference = &regulator;
}

bool LimitAlarm::isEnabled() const
{
    return settings.enabled;
}

bool LimitAlarm::isRelative(Type type) const
{
    return type == Type::DeviationHigh ||
           type == Type::DeviationLow ||
           type == Type::Band;
}

void LimitAlarm::restart()
{
    beyond = false;
    masked = settings.startupMasking;
    pending = false;
}

bool LimitAlarm::isBeyond(double_t value, double_t setpoint) const
{
    const double_t limit = settings.limit;
    const double_t hysteresis = settings.hysteresis;

    switch (settings.type)
    {
    case Type::Max:
        return beyond ? value >= limit - hysteresis : value > limit;

    case Type::Min:
        return beyond ? value <= limit + hysteresis : value < limit;

    case Type::DeviationHigh:
    {
        const double_t threshold = setpoint + limit;
        return beyond ? value >= threshold - hysteresis : value > threshold;
    }

    case Type::DeviationLow:
    {
        const double_t threshold = setpoint - limit;
        return beyond ? value <= threshold + hysteresis : value < threshold;
    }

    case Type::Band:
    {
        const double_t deviation = std::fabs(value - setpoint);
        return beyond ? deviation >= limit - hysteresis : deviation > limit;
    }
    }

    return false;
}

void LimitAlarm::update(uint32_t now)
{
    if (!settings.enabled)
    {
        // Réarmée (masquage compris) à la prochaine activation.
        started = false;
        restart();
        clearAlarm();
        return;
    }

    if (!started)
    {
        started = true;
        restart();
        clearAlarm();
    }

    pollAcknowledgeInput();

    const MeasurementStatus status =
        measurement != nullptr
            ? measurement->getStatus()
            : MeasurementStatus::Invalid;

    bool condition = false;

    if (status == MeasurementStatus::NotReady)
    {
        // Pas encore de mesure : ni alarme ni levée du masquage.
        condition = false;
    }
    else if (status != MeasurementStatus::Ok ||
             !std::isfinite(measurement->getValue()))
    {
        // Le défaut de sonde n'est pas masqué : il signale une panne.
        condition = settings.alarmOnFault;
    }
    else
    {
        double_t setpoint = 0.0;

        const bool hasSetpoint =
            !isRelative(settings.type) ||
            (reference != nullptr &&
             !reference->isInhibited() &&
             reference->readSetpoint(setpoint));

        if (!hasSetpoint)
        {
            // Régulateur arrêté : alarme relative suspendue et réarmée.
            beyond = false;
            masked = settings.startupMasking;
        }
        else
        {
            const double_t value = measurement->getValue();

            beyond = isBeyond(value, setpoint);

            if (masked && !beyond)
                masked = false;

            // Le masquage ne concerne que le côté bas (process froid à la
            // mise en route) : un dépassement haut est toujours signalé.
            const bool lowSide =
                settings.type == Type::Min ||
                settings.type == Type::DeviationLow ||
                (settings.type == Type::Band && value < setpoint);

            condition = beyond && !(masked && lowSide);
        }
    }

    bool confirmed = false;

    if (condition)
    {
        if (!pending)
        {
            pending = true;
            pendingSince = now;
        }

        confirmed = now - pendingSince >= settings.delay * 1000UL;
    }
    else
    {
        pending = false;
    }

    applyCondition(confirmed, settings.latching);
}

void LimitAlarm::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "alarms",
        "Alarmes",
        getConfigurationKey(),
        getName()
    });

    const char* unit =
        measurement != nullptr
            ? measurement->getUnit()
            : nullptr;

    parameters.addBool(
        "enabled",
        "Active",
        settings.enabled);

    if (reference != nullptr)
    {
        parameters.addSelection(
            "type",
            "Type",
            settings.type,
            ALL_TYPES);
    }
    else
    {
        parameters.addSelection(
            "type",
            "Type",
            settings.type,
            ABSOLUTE_TYPES);
    }

    parameters.addDouble(
        "limit",
        "Seuil",
        settings.limit,
        -200.0,
        1000.0,
        1.0,
        1,
        unit,
        false,
        0.1);

    parameters.addDouble(
        "hysteresis",
        "Hystérésis",
        settings.hysteresis,
        0.0,
        50.0,
        0.1,
        1,
        unit);

    parameters.addInteger(
        "delay",
        "Tempo",
        settings.delay,
        0,
        3600,
        5,
        "s");

    parameters.addBool(
        "startup_masking",
        "Masquage dém.",
        settings.startupMasking);

    parameters.addBool(
        "latching",
        "Mémorisation",
        settings.latching);

    parameters.addBool(
        "alarm_on_fault",
        "Sur défaut",
        settings.alarmOnFault);
}

bool LimitAlarm::validateParameters(
    const ParameterEditor& editor) const
{
    const ParameterDraft* type =
        editor.find(getConfigurationKey(), "type");

    if (type == nullptr)
        return true;

    // Un type relatif exige un régulateur de référence.
    return reference != nullptr ||
           !isRelative(static_cast<Type>(type->selectionValue));
}
