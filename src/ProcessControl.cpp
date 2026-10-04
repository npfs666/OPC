#include <ProcessControl.h>

#include <Measurements/Measurement.h>
#include <Inputs/DigitalInput.h>
#include <Outputs/Actuator.h>
#include <Outputs/Output.h>
#include <ProcessSnapshot.h>
#include <Regulator/Alarm.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstdio>

ProcessControl::ProcessControl()
{
    measurementCount = 0;
    regulatorCount = 0;
    actuatorCount = 0;
    outputCount = 0;
}

bool ProcessControl::add(DigitalInput& input)
{
    if (digitalInputCount >= MAX_DIGITAL_INPUTS)
        return false;

    for (uint8_t i = 0; i < digitalInputCount; i++)
    {
        if (digitalInputs[i] == &input)
            return false;
    }

    digitalInputs[digitalInputCount++] = &input;
    return true;
}

void ProcessControl::pollInputs(uint32_t now)
{
    for (uint8_t i = 0; i < digitalInputCount; i++)
        digitalInputs[i]->poll(now);
}

void ProcessControl::captureInputSnapshot(
    ProcessSnapshot& destination) const
{
    destination.digitalInputSampleCount = 0;
    destination.clockSample = clockSample;
    destination.clockNeeded = false;

    for (uint8_t i = 0; i < regulatorCount; i++)
    {
        if (regulators[i]->requiresClock())
            destination.clockNeeded = true;
    }

    for (uint8_t i = 0; i < digitalInputCount; i++)
        destination.add(*digitalInputs[i]);
}

bool ProcessControl::add(Measurement& measurement)
{
    if (measurementCount >= MAX_MEASUREMENTS)
        return false;

    for (uint8_t i = 0;
         i < measurementCount;
         i++)
    {
        if (measurements[i] == &measurement)
            return false;
    }

    measurements[measurementCount++] = &measurement;

    return true;
}

bool ProcessControl::add(Regulator& regulator)
{
    if (regulatorCount >= MAX_REGULATORS)
        return false;

    for (uint8_t i = 0;
         i < regulatorCount;
         i++)
    {
        if (regulators[i] == &regulator)
            return false;
    }

    regulators[regulatorCount++] = &regulator;

    return true;
}

bool ProcessControl::add(Alarm& alarm)
{
    if (alarmCount >= MAX_ALARMS)
        return false;

    for (uint8_t i = 0; i < alarmCount; i++)
    {
        if (alarms[i] == &alarm)
            return false;
    }

    if (!add(static_cast<Regulator&>(alarm)))
        return false;

    alarms[alarmCount++] = &alarm;
    return true;
}

void ProcessControl::acknowledgeAlarms()
{
    for (uint8_t i = 0; i < alarmCount; i++)
        alarms[i]->acknowledge();
}

namespace
{
    // Clés stables des actions « RAZ compteurs », une par sortie.
    constexpr const char* RESET_COUNTER_KEYS[] = {
        "reset_counters_1", "reset_counters_2", "reset_counters_3",
        "reset_counters_4", "reset_counters_5", "reset_counters_6",
        "reset_counters_7", "reset_counters_8", "reset_counters_9",
        "reset_counters_10", "reset_counters_11", "reset_counters_12",
        "reset_counters_13", "reset_counters_14", "reset_counters_15",
        "reset_counters_16"
    };

    static_assert(
        sizeof(RESET_COUNTER_KEYS) / sizeof(RESET_COUNTER_KEYS[0]) >=
            MAX_REGISTERED_OUTPUTS,
        "Une clé d'action par sortie");
}

bool ProcessControl::addMenuActions(MenuBuilder& menu) const
{
    if (alarmCount > 0)
    {
        const MenuBuilder::GroupId group =
            menu.findSubmenu(menu.root(), "alarms");

        if (group == MenuBuilder::INVALID_GROUP ||
            !menu.addAction(
                group,
                ACKNOWLEDGE_ALARMS_ACTION,
                "acknowledge_alarms",
                "Acquitter"))
        {
            return false;
        }
    }

    for (uint8_t i = 0; i < outputCount; i++)
    {
        const char* ownerKey = outputs[i]->countersOwnerKey();

        if (ownerKey == nullptr)
            continue;

        const MenuBuilder::GroupId group =
            menu.findGroupForOwner(ownerKey);

        if (group == MenuBuilder::INVALID_GROUP ||
            !menu.addAction(
                group,
                RESET_COUNTERS_ACTION + i,
                RESET_COUNTER_KEYS[i],
                "RAZ compteurs"))
        {
            return false;
        }
    }

    return true;
}

bool ProcessControl::handlesMenuAction(
    MenuBuilder::ActionId actionId) const
{
    if (actionId == ACKNOWLEDGE_ALARMS_ACTION)
        return alarmCount > 0;

    if (actionId >= RESET_COUNTERS_ACTION &&
        actionId < RESET_COUNTERS_ACTION + outputCount)
    {
        return outputs[actionId - RESET_COUNTERS_ACTION]->counters() !=
               nullptr;
    }

    return false;
}

bool ProcessControl::executeMenuAction(
    MenuBuilder::ActionId actionId)
{
    if (!handlesMenuAction(actionId))
        return false;

    if (actionId == ACKNOWLEDGE_ALARMS_ACTION)
    {
        acknowledgeAlarms();
        return true;
    }

    const uint8_t index = actionId - RESET_COUNTERS_ACTION;
    outputs[index]->resetCounters();
    maintenanceLogged[index] = false;
    countersChanged = true;
    return true;
}

void ProcessControl::updateOperatingTime(uint32_t now)
{
    if (operatingTickStarted)
        operatingSecondsTotal += (now - lastOperatingTick) / 1000.0;

    operatingTickStarted = true;
    lastOperatingTick = now;
    operatingHoursDisplay = operatingSecondsTotal / 3600.0;
}

double_t ProcessControl::operatingSeconds() const
{
    return operatingSecondsTotal;
}

void ProcessControl::restoreOperatingSeconds(double_t seconds)
{
    if (std::isfinite(seconds) && seconds > 0.0)
        operatingSecondsTotal = seconds;

    operatingHoursDisplay = operatingSecondsTotal / 3600.0;
}

size_t ProcessControl::registeredOutputCount() const
{
    return outputCount;
}

Output* ProcessControl::registeredOutput(size_t index)
{
    return index < outputCount ? outputs[index] : nullptr;
}

bool ProcessControl::takeCountersChanged()
{
    const bool changed = countersChanged;
    countersChanged = false;
    return changed;
}

bool ProcessControl::add(Actuator& actuator)
{
    if (actuatorCount >= MAX_ACTUATORS)
        return false;

    for (uint8_t i = 0;
         i < actuatorCount;
         i++)
    {
        if (actuators[i] == &actuator)
            return false;
    }

    actuators[actuatorCount++] = &actuator;

    return true;
}

bool ProcessControl::connect(
    Actuator& actuator,
    Output& output)
{
    bool actuatorRegistered = false;

    for (uint8_t i = 0;
         i < actuatorCount;
         i++)
    {
        if (actuators[i] == &actuator)
        {
            actuatorRegistered = true;
            break;
        }
    }

    if (!actuatorRegistered)
        return false;

    if (outputCount >= MAX_REGISTERED_OUTPUTS)
        return false;

    for (uint8_t i = 0;
         i < outputCount;
         i++)
    {
        if (outputs[i] == &output)
            return false;
    }

    if (!actuator.addOutput(output))
        return false;

    outputs[outputCount++] = &output;

    return true;
}

void ProcessControl::updateMeasurementsAndRegulators(
    uint32_t now)
{
    pollInputs(now);

    // Chaque mesure est filtrée avant d'être lue par les mesures calculées.
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        if (measurements[i] != nullptr)
        {
            measurements[i]->update();
            measurements[i]->applyFilter(now);
        }
    }

    recordStatusChanges(now);

    for (uint8_t i = 0; i < regulatorCount; i++)
        regulators[i]->update(now);

    recordAlarmChanges(now);
    recordMaintenanceChanges(now);

    poll(now);
}

void ProcessControl::recordMaintenanceChanges(uint32_t now)
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        const OutputCounters* counters = outputs[i]->counters();
        const bool due = counters != nullptr && counters->maintenanceDue();

        if (due == maintenanceLogged[i])
            continue;

        maintenanceLogged[i] = due;

        if (!due)
            continue;

        if (maintenanceEventCount >= MAX_STATUS_EVENTS)
        {
            if (lostStatusEvents < UINT16_MAX)
                lostStatusEvents++;
            continue;
        }

        maintenanceEvents[maintenanceEventCount++] =
            {now, i, counters->switches};
    }
}

ProcessControl::AlarmState ProcessControl::alarmState(uint8_t index) const
{
    const Alarm& alarm = *alarms[index];

    if (alarm.isLatched())
        return AlarmState::Latched;

    return alarm.isActive() ? AlarmState::Active : AlarmState::Clear;
}

void ProcessControl::recordAlarmChanges(uint32_t now)
{
    for (uint8_t i = 0; i < alarmCount; i++)
    {
        const AlarmState state = alarmState(i);

        if (state == loggedAlarmState[i])
            continue;

        loggedAlarmState[i] = state;

        if (alarmEventCount >= MAX_STATUS_EVENTS)
        {
            if (lostStatusEvents < UINT16_MAX)
                lostStatusEvents++;
            continue;
        }

        alarmEvents[alarmEventCount++] = {now, i, state};
    }
}

void ProcessControl::recordStatusChanges(uint32_t now)
{
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        if (measurements[i] == nullptr)
            continue;

        const MeasurementStatus status =
            measurements[i]->getStatus();

        const MeasurementStatus previous = loggedStatus[i];

        if (status == previous)
            continue;

        loggedStatus[i] = status;

        if (previous == MeasurementStatus::NotReady &&
            status == MeasurementStatus::Ok)
            continue;

        if (statusEventCount >= MAX_STATUS_EVENTS)
        {
            if (lostStatusEvents < UINT16_MAX)
                lostStatusEvents++;
            continue;
        }

        statusEvents[statusEventCount++] = {now, i, previous, status};
    }
}

void ProcessControl::printStatusEvents(Stream& stream)
{
    char line[96];

    for (uint8_t i = 0; i < statusEventCount; i++)
    {
        const StatusEvent& event = statusEvents[i];

        std::snprintf(
            line,
            sizeof(line),
            "[%lu ms] %s : %s -> %s",
            static_cast<unsigned long>(event.time),
            measurements[event.measurement]->getName(),
            measurementStatusLabel(event.from),
            measurementStatusLabel(event.to));

        stream.println(line);
    }

    for (uint8_t i = 0; i < alarmEventCount; i++)
    {
        const AlarmEvent& event = alarmEvents[i];

        std::snprintf(
            line,
            sizeof(line),
            "[%lu ms] Alarme %s : %s",
            static_cast<unsigned long>(event.time),
            alarms[event.alarm]->getName(),
            event.state == AlarmState::Active
                ? "ACTIVE"
                : event.state == AlarmState::Latched
                    ? "MEMORISEE"
                    : "FIN");

        stream.println(line);
    }

    for (uint8_t i = 0; i < maintenanceEventCount; i++)
    {
        const MaintenanceEvent& event = maintenanceEvents[i];

        std::snprintf(
            line,
            sizeof(line),
            "[%lu ms] Entretien %s : seuil atteint (%lu manoeuvres)",
            static_cast<unsigned long>(event.time),
            outputs[event.output]->getName(),
            static_cast<unsigned long>(event.switches));

        stream.println(line);
    }

    if (lostStatusEvents > 0)
    {
        std::snprintf(
            line,
            sizeof(line),
            "(%u changement(s) d'etat non journalise(s))",
            static_cast<unsigned>(lostStatusEvents));

        stream.println(line);
    }

    statusEventCount = 0;
    alarmEventCount = 0;
    maintenanceEventCount = 0;
    lostStatusEvents = 0;
}

void ProcessControl::poll(uint32_t now)
{
    for (uint8_t i = 0; i < actuatorCount; i++)
        actuators[i]->update(now);

    for (uint8_t i = 0; i < outputCount; i++)
        outputs[i]->poll(now);
}

void ProcessControl::resume(uint32_t now)
{
    for (uint8_t i = 0; i < regulatorCount; i++)
        regulators[i]->resume(now);

    for (uint8_t i = 0; i < actuatorCount; i++)
        actuators[i]->resume(now);
}

void ProcessControl::updateClock(const ClockSample& sample)
{
    clockSample = sample;
}

const ClockSample& ProcessControl::clock() const
{
    return clockSample;
}

bool ProcessControl::beginOutputs()
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] == nullptr ||
            !outputs[i]->begin())
        {
            forceSafeOutputs();
            return false;
        }
    }

    forceSafeOutputs();

    return true;
}

bool ProcessControl::applyOutputSettings()
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] == nullptr ||
            !outputs[i]->applySettings())
        {
            forceSafeOutputs();
            return false;
        }
    }

    forceSafeOutputs();

    return true;
}

void ProcessControl::forceSafeOutputs()
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr)
            outputs[i]->forceSafe();
    }
}

bool ProcessControl::outputsHealthy() const
{
    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] == nullptr ||
            !outputs[i]->isHealthy())
        {
            return false;
        }
    }

    return true;
}

void ProcessControl::captureSnapshot(
    ProcessSnapshot& destination,
    uint32_t now) const
{
    destination.clear(now);
    captureInputSnapshot(destination);

    for (uint8_t i = 0; i < measurementCount; i++)
    {
        if (measurements[i] != nullptr)
            destination.add(*measurements[i]);
    }

    for (uint8_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr)
            destination.add(*outputs[i]);
    }

    for (uint8_t i = 0; i < alarmCount; i++)
        destination.add(*alarms[i]);
}

/*Measurement* ProcessControl::getMeasurement(uint8_t id) {

    if (id >= measurementCount)
        return nullptr;

    return measurements[id];
}*/

/**
 * Only for testing hardware, do not correct
 */
void ProcessControl::printCSVPsychro(Stream& stream) const {

    stream.print(measurements[2]->getValue(),3);
    stream.print(";");
    stream.print(measurements[4]->getValue(),3);
    stream.print(";");
    stream.println(measurements[5]->getValue(),3);
}

void ProcessControl::print(Stream& stream) const
{
    //stream.println();
    stream.println(F("===== Process Control ====="));

    if (digitalInputCount > 0)
        stream.println(F("-------Digital inputs-----"));

    for (uint8_t i = 0; i < digitalInputCount; i++)
        digitalInputs[i]->print(stream);

    stream.println(F("-------Measurements-------"));
    for (uint8_t i = 0; i < measurementCount; i++)
    {
        measurements[i]->print(stream);
    }
    
    if (regulatorCount > 0 )  {
        stream.println(F("--------Regulators--------"));
    }
    
    for (uint8_t i = 0; i < regulatorCount; i++)
    {
        regulators[i]->print(stream);
    }

    if (actuatorCount > 0 )  {
        stream.println(F("--------Actuators---------"));
    }
    for (uint8_t i = 0; i < actuatorCount; i++)
    {
        actuators[i]->print(stream);
    }

    if (outputCount > 0)
    {
        stream.println(F("----------Outputs---------"));
    }

    for (uint8_t i = 0; i < outputCount; i++)
    {
        outputs[i]->print(stream);
    }

    stream.println(F("=========================="));
    stream.println();
}

void ProcessControl::registerParameters(ParameterList& list)
{
    // Divers > Compteurs, parent des compteurs de chaque relais : enregistré
    // avant les sorties.
    auto counters = list.forOwner({
        "miscellaneous",
        "Divers",
        "counters",
        "Compteurs",
        false
    });

    counters.addDouble(
        "operating_hours",
        "Heures carte",
        operatingHoursDisplay,
        "h",
        true,
        1);

    for (uint8_t i = 0; i < digitalInputCount; i++)
        digitalInputs[i]->registerParameters(list);

    for (size_t i = 0; i < regulatorCount; i++)
    {
        regulators[i]->registerParameters(list);
    }

    for (size_t i = 0; i < actuatorCount; i++)
    {
        actuators[i]->registerParameters(list);
    }

    for (size_t i = 0; i < outputCount; i++)
    {
        outputs[i]->registerParameters(list);
    }
}

bool ProcessControl::validateParameters(
    const ParameterEditor& editor) const
{
    for (size_t i = 0; i < regulatorCount; i++)
    {
        if (regulators[i] != nullptr &&
            !regulators[i]->validateParameters(
                editor))
        {
            return false;
        }
    }

    for (size_t i = 0; i < actuatorCount; i++)
    {
        if (actuators[i] != nullptr &&
            !actuators[i]->validateParameters(
                editor))
        {
            return false;
        }
    }

    for (size_t i = 0; i < outputCount; i++)
    {
        if (outputs[i] != nullptr &&
            !outputs[i]->validateParameters(
                editor))
        {
            return false;
        }
    }

    return true;
}
