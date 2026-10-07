#include <Regulator/LoopBreakAlarm.h>

#include <Measurements/Measurement.h>
#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    // Tolérance de comparaison de la commande avec ses butées.
    constexpr double_t SATURATION_TOLERANCE = 0.001;
}

void LoopBreakAlarm::begin(
    const char* key,
    const char* name,
    Measurement& measurement,
    Regulator& regulator)
{
    Regulator::begin(key, name);
    beginAlarm();
    this->measurement = &measurement;
    this->regulator = &regulator;
    started = false;
    armedSaturation = 0;

    regulator.setInterlock(*this);
}

bool LoopBreakAlarm::isEnabled() const
{
    return settings.enabled;
}

bool LoopBreakAlarm::locksOutputs() const
{
    // En manuel, l'opérateur garde la main (comme sur défaut de sonde) ;
    // de retour en Auto, la sécurité reprend jusqu'à l'acquittement.
    return settings.enabled &&
           settings.safeState &&
           isActive() &&
           regulator != nullptr &&
           regulator->isAutomatic();
}

uint32_t LoopBreakAlarm::detectionTimeMs() const
{
    if (settings.detectionTime > 0)
        return settings.detectionTime * 1000UL;

    const double_t ti =
        regulator != nullptr ? regulator->integralTime() : 0.0;

    if (!(ti > 0.0))
        return DEFAULT_DETECTION_S * 1000UL;

    const double_t seconds = std::fmax(
        2.0 * ti,
        static_cast<double_t>(AUTOMATIC_MINIMUM_S));

    return static_cast<uint32_t>(seconds * 1000.0);
}

int8_t LoopBreakAlarm::saturation(double_t value, double_t setpoint) const
{
    double_t minimum = 0.0;
    double_t maximum = 1.0;
    regulator->readOutputLimits(minimum, maximum);

    const double_t command = regulator->readCommand();

    // Écart dans le sens de l'action : positif quand la commande doit
    // faire monter la mesure (chauffage sous la consigne).
    const double_t demand =
        regulator->actionDirection() * (setpoint - value);

    if (command >= maximum - SATURATION_TOLERANCE &&
        demand > settings.minimumChange)
    {
        return 1;
    }

    if (command <= minimum + SATURATION_TOLERANCE &&
        demand < -settings.minimumChange)
    {
        return -1;
    }

    return 0;
}

void LoopBreakAlarm::update(uint32_t now)
{
    if (!settings.enabled)
    {
        started = false;
        armedSaturation = 0;
        clearAlarm();
        return;
    }

    if (!started)
    {
        started = true;
        armedSaturation = 0;
        clearAlarm();
    }

    pollAcknowledgeInput();

    double_t setpoint = 0.0;

    // Boucle en régulation automatique normale uniquement. Un régulateur
    // verrouillé par cette alarme n'est plus surveillé : l'alarme mémorisée
    // reste signalée jusqu'à l'acquittement.
    // Inhibée par la glue : pas de surveillance, la fenêtre repart à zéro.
    const bool monitored =
        !isInhibited() &&
        regulator != nullptr &&
        measurement != nullptr &&
        regulator->isAutomatic() &&
        !regulator->isInhibited() &&
        regulator->isCommandValid() &&
        measurement->isValid() &&
        std::isfinite(measurement->getValue()) &&
        regulator->readSetpoint(setpoint);

    const int8_t current =
        monitored
            ? saturation(measurement->getValue(), setpoint)
            : 0;

    bool confirmed = false;

    if (current == 0)
    {
        armedSaturation = 0;
    }
    else
    {
        const double_t value = measurement->getValue();

        if (current != armedSaturation)
        {
            armedSaturation = current;
            windowStart = now;
            windowValue = value;
            windowElapsed = false;
        }

        // Rapprochement de la consigne depuis le début de la fenêtre.
        const double_t progress =
            current * regulator->actionDirection() * (value - windowValue);

        if (progress >= settings.minimumChange)
        {
            windowStart = now;
            windowValue = value;
            windowElapsed = false;
        }

        if (!windowElapsed &&
            now - windowStart >= detectionTimeMs())
        {
            windowElapsed = true;
        }

        confirmed = windowElapsed;
    }

    applyCondition(confirmed, settings.latching);
}

void LoopBreakAlarm::registerParameters(ParameterList& list)
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

    // 0 = automatique (2 × Ti, au moins 60 s ; 600 s sans Ti).
    parameters.addInteger(
        "detection_time",
        "Temps détect.",
        settings.detectionTime,
        0,
        7200,
        30,
        "s");

    parameters.addDouble(
        "minimum_change",
        "Variation min.",
        settings.minimumChange,
        0.1,
        50.0,
        0.5,
        1,
        unit,
        false,
        0.1);

    parameters.addBool(
        "latching",
        "Mémorisation",
        settings.latching);

    parameters.addBool(
        "safe_state",
        "Mise en sécu.",
        settings.safeState);
}
