#ifndef PROCESSCONTROL_H
#define PROCESSCONTROL_H

#include <Arduino.h>

#include <Hardware/pinout.h>
#include <Hardware/RTC.h>
#include <EventLog.h>
#include <Measurements/MeasurementStatus.h>
#include <hmi/MenuBuilder.h>

class Actuator;
class DigitalInput;
class Alarm;
class ConditionAlarm;
class LogicCommand;
class Measurement;
class Output;
class ParameterEditor;
class ParameterList;
class ProcessLogic;
class ProcessSnapshot;
class Regulator;

class ProcessControl
{
public:

    ProcessControl();

    bool add(Measurement& measurement);
    bool add(DigitalInput& input);
    bool add(Regulator& regulator);
    bool add(Actuator& actuator);

    /** Une alarme est un régulateur, suivi en plus pour l'affichage. */
    bool add(Alarm& alarm);

    /**
     * Une commande de la glue est un régulateur, appliquée en plus après
     * la glue (voir setLogic()).
     */
    bool add(LogicCommand& command);

    /**
     * Une alarme sur condition est une alarme ; écrite par la glue, elle est
     * évaluée en plus après la glue.
     */
    bool add(ConditionAlarm& alarm);

    /**
     * Glue de l'installation, appelée à chaque cycle après les régulateurs et
     * les alarmes, avant les actionneurs. Enregistrée par OPC.
     */
    void setLogic(ProcessLogic& logic);

    /** Efface la mémorisation de toutes les alarmes. */
    void acknowledgeAlarms();

    /** Action « Acquitter » du menu Alarmes, si l'installation en a. */
    static constexpr MenuBuilder::ActionId ACKNOWLEDGE_ALARMS_ACTION = 48;

    /** Action « RAZ compteurs » de la sortie d'indice i : base + i. */
    static constexpr MenuBuilder::ActionId RESET_COUNTERS_ACTION = 49;

    bool addMenuActions(MenuBuilder& menu) const;
    bool handlesMenuAction(MenuBuilder::ActionId actionId) const;
    bool executeMenuAction(MenuBuilder::ActionId actionId);

    /**
     * Relie une sortie à un actionneur déjà enregistré.
     * Une même sortie ne peut être reliée qu'une seule fois.
     */
    bool connect(
        Actuator& actuator,
        Output& output);

    void updateMeasurementsAndRegulators(
        uint32_t now);

    void poll(uint32_t now);

    // Acquisition indépendante du cycle ADC et de l'activation des sorties.
    void pollInputs(uint32_t now);

    // Actualise les entrées et l'heure, sans modifier les mesures/sorties.
    void captureInputSnapshot(ProcessSnapshot& destination) const;

    void resume(uint32_t now);

    /** Mise à jour par OPC, sur le cœur contrôle, environ chaque seconde. */
    void updateClock(const ClockSample& sample);

    /** Heure courante pour les régulateurs horaires (TimeSchedule). */
    const ClockSample& clock() const;

    bool beginOutputs();

    bool applyOutputSettings();

    void forceSafeOutputs();

    bool outputsHealthy() const;

    //Measurement* getMeasurement(uint8_t id);

    void captureSnapshot(
        ProcessSnapshot& destination,
        uint32_t now) const;

    void print(Stream& stream) const;

    void printCSVPsychro(Stream& stream) const;

    /**
     * Écrit les événements ajoutés au journal depuis le dernier appel
     * (changements d'état des mesures et des alarmes, entretien...). Le
     * passage NotReady -> Ok du démarrage n'est pas journalisé.
     */
    void printStatusEvents(Stream& stream);

    /** Journal des événements (sous processDataMutex). */
    EventLog& eventLog();
    const EventLog& eventLog() const;

    /** Ajoute au journal un événement à l'heure courante (format printf). */
    void logEvent(
        uint32_t now,
        EventKind kind,
        bool important,
        const char* format,
        ...);

    // ----- Compteurs d'entretien -----

    /** Cœur contrôle, à chaque tour : durée de fonctionnement de la carte. */
    void updateOperatingTime(uint32_t now);

    double_t operatingSeconds() const;
    void restoreOperatingSeconds(double_t seconds);

    /** Sorties enregistrées, pour la sauvegarde des compteurs. */
    size_t registeredOutputCount() const;
    Output* registeredOutput(size_t index);

    /** Vrai une fois après une remise à zéro : compteurs à sauvegarder. */
    bool takeCountersChanged();

    void registerParameters(ParameterList& list);

    bool validateParameters(
        const ParameterEditor& editor) const;

private:

    void recordStatusChanges(uint32_t now);

    // État d'une alarme pour le journal.
    enum class AlarmState : uint8_t
    {
        Clear,
        Active,
        Latched
    };

    void recordAlarmChanges(uint32_t now);
    void recordMaintenanceChanges(uint32_t now);
    AlarmState alarmState(uint8_t index) const;

    Alarm* alarms[MAX_ALARMS] = {};
    uint8_t alarmCount = 0;

    ProcessLogic* logic = nullptr;

    LogicCommand* logicCommands[MAX_REGULATORS] = {};
    uint8_t logicCommandCount = 0;

    ConditionAlarm* conditionAlarms[MAX_ALARMS] = {};
    uint8_t conditionAlarmCount = 0;
    AlarmState loggedAlarmState[MAX_ALARMS] = {};

    // Seuils d'entretien atteints (journalisés une fois par sortie).
    bool maintenanceLogged[MAX_REGISTERED_OUTPUTS] = {};

    EventLog events;

    // Durée de fonctionnement de la carte, et copie affichée dans le menu.
    double_t operatingSecondsTotal = 0.0;
    double_t operatingHoursDisplay = 0.0;
    uint32_t lastOperatingTick = 0;
    bool operatingTickStarted = false;
    bool countersChanged = false;

    ClockSample clockSample;

    MeasurementStatus loggedStatus[MAX_MEASUREMENTS] = {};

    DigitalInput* digitalInputs[MAX_DIGITAL_INPUTS] = {};
    uint8_t digitalInputCount = 0;

    Measurement* measurements[MAX_MEASUREMENTS];
    uint8_t measurementCount;

    Regulator* regulators[MAX_REGULATORS];
    uint8_t regulatorCount;

    Actuator* actuators[MAX_ACTUATORS];
    uint8_t actuatorCount;

    Output* outputs[MAX_REGISTERED_OUTPUTS];
    uint8_t outputCount;
};

#endif
