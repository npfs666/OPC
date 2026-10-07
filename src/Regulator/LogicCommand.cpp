#include <Regulator/LogicCommand.h>

#include <Arduino.h>
#include <Inputs/DigitalInput.h>
#include <Measurements/Measurement.h>
#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    constexpr ParameterOption LOGIC_OPERATION_OPTIONS[] = {
        {static_cast<int32_t>(LogicCommand::Operation::Auto), "Auto"},
        {static_cast<int32_t>(LogicCommand::Operation::ForcedOn), "Marche"},
        {static_cast<int32_t>(LogicCommand::Operation::ForcedOff), "Arrêt"}
    };
}

void LogicCommand::begin(const char* name)
{
    begin(name, name);
}

void LogicCommand::begin(
    const char* key,
    const char* name)
{
    Regulator::begin(key, name);

    settings.operation = Operation::Auto;
    dependencyCount = 0;
    manualModeAvailable = true;
    written = false;
    requestedValid = false;
    requested = 0.0;
    missing = false;
}

bool LogicCommand::addDependency(const Dependency& dependency)
{
    if (dependencyCount >= MAX_DEPENDENCIES)
        return false;

    dependencies[dependencyCount++] = dependency;
    return true;
}

bool LogicCommand::dependsOn(const Measurement& measurement)
{
    Dependency dependency;
    dependency.measurement = &measurement;
    return addDependency(dependency);
}

bool LogicCommand::dependsOn(const DigitalInput& input)
{
    Dependency dependency;
    dependency.input = &input;
    return addDependency(dependency);
}

bool LogicCommand::dependsOn(const Regulator& block)
{
    Dependency dependency;
    dependency.block = &block;
    return addDependency(dependency);
}

void LogicCommand::disableManualMode()
{
    manualModeAvailable = false;
    settings.operation = Operation::Auto;
}

void LogicCommand::set(double_t value)
{
    written = true;
    requestedValid = std::isfinite(value);
    requested = requestedValid ? value : 0.0;
}

void LogicCommand::setOn(bool on)
{
    set(on ? 1.0 : 0.0);
}

void LogicCommand::invalidate()
{
    written = true;
    requestedValid = false;
    requested = 0.0;
}

void LogicCommand::update(uint32_t now)
{
    (void)now;
}

void LogicCommand::resume(uint32_t now)
{
    written = false;
    requestedValid = false;
    requested = 0.0;
    missing = false;
    Regulator::resume(now);
}

bool LogicCommand::isAutomatic() const
{
    return !manualModeAvailable ||
           settings.operation == Operation::Auto;
}

bool LogicCommand::isManual() const
{
    return !isAutomatic();
}

MeasurementStatus LogicCommand::dependencyStatus() const
{
    for (uint8_t i = 0; i < dependencyCount; i++)
    {
        const Dependency& dependency = dependencies[i];

        if (dependency.measurement != nullptr)
        {
            const MeasurementStatus status =
                dependency.measurement->getStatus();

            if (status != MeasurementStatus::Ok)
                return status;

            if (!std::isfinite(dependency.measurement->getValue()))
                return MeasurementStatus::Invalid;
        }
        else if (dependency.input != nullptr)
        {
            if (!dependency.input->isValid())
                return MeasurementStatus::Invalid;
        }
        else if (dependency.block != nullptr &&
                 !dependency.block->isCommandValid())
        {
            return MeasurementStatus::Invalid;
        }
    }

    return MeasurementStatus::Ok;
}

bool LogicCommand::applyLogic(uint32_t now)
{
    const bool wasWritten = written;
    written = false;

    /*
     * Mode manuel : ni glue ni dépendances, comme le thermostat. Les sécurités
     * des sorties restent actives.
     */
    if (!isAutomatic())
    {
        missing = false;
        writeCommand(
            settings.operation == Operation::ForcedOn ? 1.0 : 0.0);
        return false;
    }

    // Dépendance en défaut : état sûr ou maintien, sans chercher d'oubli
    // (une glue peut ne rien écrire quand ses mesures sont en défaut).
    const MeasurementStatus status = dependencyStatus();

    if (status != MeasurementStatus::Ok)
    {
        handleMeasurementFault(now, status);
        return false;
    }

    if (!wasWritten)
    {
        const bool started = !missing;
        missing = true;
        invalidateCommand();
        return started;
    }

    missing = false;

    if (requestedValid)
        writeCommand(requested);
    else
        invalidateCommand();

    return false;
}

void LogicCommand::registerParameters(ParameterList& list)
{
    // Mode manuel en tête du menu, non sauvegardé (retour en Auto au
    // démarrage).
    if (manualModeAvailable)
    {
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
            LOGIC_OPERATION_OPTIONS);

        // Réglage de conduite : appliqué sans arrêter la régulation.
        list.setLive(getConfigurationKey(), "operation");
    }

    // Sans dépendance, aucun défaut à traiter.
    if (dependencyCount > 0)
        registerFaultParameters(list);
}
