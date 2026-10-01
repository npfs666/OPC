#ifndef PROCESSCONTROL_H
#define PROCESSCONTROL_H

#include <Arduino.h>

#include <Hardware/pinout.h>
#include <Hardware/RTC.h>
#include <Measurements/MeasurementStatus.h>

class Actuator;
class DigitalInput;
class Measurement;
class Output;
class ParameterEditor;
class ParameterList;
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
     * Écrit une ligne par changement d'état de mesure survenu depuis le
     * dernier appel, puis les oublie. Le passage NotReady -> Ok du démarrage
     * n'est pas journalisé.
     */
    void printStatusEvents(Stream& stream);

    void registerParameters(ParameterList& list);

    bool validateParameters(
        const ParameterEditor& editor) const;

private:

    struct StatusEvent
    {
        uint32_t time;
        uint8_t measurement;
        MeasurementStatus from;
        MeasurementStatus to;
    };

    // Au-delà, les changements suivants sont seulement comptés : les premiers
    // désignent en général la cause.
    static constexpr uint8_t MAX_STATUS_EVENTS = 8;

    void recordStatusChanges(uint32_t now);

    ClockSample clockSample;

    MeasurementStatus loggedStatus[MAX_MEASUREMENTS] = {};
    StatusEvent statusEvents[MAX_STATUS_EVENTS] = {};
    uint8_t statusEventCount = 0;
    uint16_t lostStatusEvents = 0;

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
