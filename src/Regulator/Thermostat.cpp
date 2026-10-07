#include <Regulator/Thermostat.h>

#include <Arduino.h>
#include <Measurements/Temperature/Temperature.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    constexpr ParameterOption THERMOSTAT_OPERATION_OPTIONS[] = {
        {static_cast<int32_t>(Thermostat::Operation::Auto), "Auto"},
        {static_cast<int32_t>(Thermostat::Operation::ForcedOn), "Marche"},
        {static_cast<int32_t>(Thermostat::Operation::ForcedOff), "Arrêt"}
    };

    constexpr ParameterOption THERMOSTAT_MODE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Thermostat::Mode::Heating),
            "Chauffage"
        },
        {
            static_cast<int32_t>(
                Thermostat::Mode::Cooling),
            "Refroidissement"
        }
    };
}

Thermostat::Thermostat()
{
}

void Thermostat::begin(const char* name,Temperature& temperature)
{
    begin(name, name, temperature);
}

void Thermostat::begin(
    const char* key,
    const char* name,
    Temperature& temperature)
{
    Regulator::begin(key, name);

    this->temperature = &temperature;

    settings.mode = Mode::Heating;
    settings.setpoint = 20.0;
    settings.hysteresis = 1.0;
    setpointRamp.begin();
    scheduledSetpoint.begin();
}

void Thermostat::setSchedule(
    const TimeSchedule& schedule,
    double_t reducedSetpoint)
{
    scheduledSetpoint.attach(schedule, reducedSetpoint);
}

void Thermostat::update(uint32_t now)
{
    /*
     * Mode manuel : ni mesure ni repli. Au retour en Auto, la sortie forcée
     * est conservée dans la bande d'hystérésis (pas d'à-coup) et la rampe
     * repart de la mesure.
     */
    if (settings.operation != Operation::Auto)
    {
        setpointRamp.restart();
        writeCommand(
            settings.operation == Operation::ForcedOn ? 1.0 : 0.0);
        return;
    }

    /*
     * Inhibé par la glue : arrêt commandé (commande 0, voir Regulator). À la
     * levée, départ à l'arrêt comme après le menu, rampe repartie de la
     * mesure.
     */
    if (isInhibited())
    {
        setpointRamp.restart();
        invalidateCommand();
        return;
    }

    if (temperature == nullptr ||
        !temperature->isValid() ||
        !std::isfinite(
            temperature->getValue()))
    {
        setpointRamp.resume(now);
        handleMeasurementFault(
            now,
            temperature != nullptr
                ? temperature->getStatus()
                : MeasurementStatus::Invalid);
        return;
    }

    const double_t value =
        temperature->getValue();

    double_t target = settings.setpoint;

    if (!scheduledSetpoint.update(
            settings.setpoint,
            target))
    {
        setpointRamp.restart();
        invalidateCommand();
        return;
    }

    if (!setpointRamp.update(
            now,
            target,
            value))
    {
        invalidateCommand();
        return;
    }

    const double_t activeSetpoint =
        setpointRamp.activeSetpoint();

    const double_t lowerThreshold =
        activeSetpoint -
        settings.hysteresis / 2.0;

    const double_t upperThreshold =
        activeSetpoint +
        settings.hysteresis / 2.0;

    /*
     * Dans la bande, l'état précédent est conservé. Sans état précédent
     * (démarrage, retour du menu ou d'une mesure valide), la sortie part à
     * l'arrêt plutôt que de rester invalide jusqu'au prochain seuil.
     */
    switch (settings.mode)
    {
    case Mode::Heating:
        if (value <= lowerThreshold)
            writeCommand(1.0);
        else if (value >= upperThreshold || !isCommandValid())
            writeCommand(0.0);
        break;

    case Mode::Cooling:
        if (value >= upperThreshold)
            writeCommand(1.0);
        else if (value <= lowerThreshold || !isCommandValid())
            writeCommand(0.0);
        break;

    default:
        invalidateCommand();
        break;
    }
}

int8_t Thermostat::actionDirection() const
{
    return settings.mode == Mode::Cooling ? -1 : 1;
}

bool Thermostat::isAutomatic() const
{
    return settings.operation == Operation::Auto;
}

bool Thermostat::isManual() const
{
    return settings.operation != Operation::Auto;
}

bool Thermostat::readSetpoint(double_t& setpoint) const
{
    // Inhibé : pas de consigne active, les alarmes relatives sont suspendues.
    if (isInhibited() ||
        !setpointRamp.hasActiveSetpoint())
        return false;

    setpoint = setpointRamp.activeSetpoint();
    return true;
}

void Thermostat::resume(uint32_t now)
{
    Regulator::resume(now);
    setpointRamp.resume(now);
}

void Thermostat::print(Stream& stream) const {
    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");

    stream.print((command == 0) ? "Off" : "On");
    stream.print(" | SP : ");
    stream.print(
        setpointRamp.hasActiveSetpoint()
            ? setpointRamp.activeSetpoint()
            : settings.setpoint);

    if (setpointRamp.settings.enabled)
    {
        stream.print(" | Target : ");
        stream.print(settings.setpoint);
    }

    stream.print(" | Hyst : ");
    stream.print(settings.hysteresis);

    if (scheduledSetpoint.isAttached())
    {
        stream.print(" | Prog : ");
        stream.print(
            ScheduledSetpoint::stateName(
                scheduledSetpoint.state()));
    }

    stream.println(' ');
}

void Thermostat::registerParameters(ParameterList& list) {

    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    });

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
        THERMOSTAT_OPERATION_OPTIONS);

    parameters.addSelection(
        "mode",
        "Mode",
        settings.mode,
        THERMOSTAT_MODE_OPTIONS);

    parameters.addDouble(
        "setpoint",
        "Consigne",
        settings.setpoint,
        0.0,
        200.0,
        0.1,
        1,
        "°C");

    scheduledSetpoint.registerParameters(
        parameters,
        0.0,
        200.0,
        0.1,
        1,
        "°C");

    parameters.addDouble(
        "hysteresis",
        "Hystérésis",
        settings.hysteresis,
        0.1,
        10.0,
        0.1,
        1,
        "°C");

    registerFaultParameters(list);

    // Réglages de conduite : appliqués sans arrêter la régulation.
    list.setLive(getConfigurationKey(), "operation");
    list.setLive(getConfigurationKey(), "setpoint");
    list.setLive(getConfigurationKey(), "reduced_setpoint");
}
