#ifndef PROCESS_SNAPSHOT_H
#define PROCESS_SNAPSHOT_H

#include <Hardware/pinout.h>
#include <Hardware/RTC.h>
#include <Measurements/MeasurementStatus.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

class Measurement;
class DigitalInput;
class LimitAlarm;
class Output;
class ProcessControl;
class ProcessSnapshot;

struct DigitalInputSample
{
    bool active = false;
    bool valid = false;
    // Date de dernière lecture, indépendante de capturedAt().
    uint32_t sampledAt = 0;

private:
    friend class ProcessSnapshot;

    const DigitalInput* source = nullptr;
};

struct MeasurementSample
{
    double_t value = 0.0;
    const char* unit = "";
    uint8_t decimals = 3;
    // valid vaut status == MeasurementStatus::Ok.
    bool valid = false;
    MeasurementStatus status = MeasurementStatus::NotReady;

private:
    friend class ProcessSnapshot;

    const Measurement* source = nullptr;
};

struct OutputSample
{
    double_t appliedCommand = 0.0;
    bool healthy = false;
    // Commande demandée retardée par un temps minimal (Output::isWaiting()).
    bool waiting = false;

    /** Arrêtée, mise en marche retardée par un temps minimal d'arrêt. */
    bool waitingToStart() const
    {
        return healthy && waiting && appliedCommand < 0.5;
    }

private:
    friend class ProcessSnapshot;

    const Output* source = nullptr;
};

struct AlarmSample
{
    const char* name = "";
    bool enabled = false;
    // Signalée : en cours ou mémorisée.
    bool active = false;
    // Mémorisée : cause disparue, acquittement attendu.
    bool latched = false;

private:
    friend class ProcessSnapshot;

    const LimitAlarm* source = nullptr;
};

class ProcessSnapshot
{
public:
    size_t inputCount() const;

    const DigitalInputSample* inputAt(size_t index) const;

    const DigitalInputSample* find(const DigitalInput& input) const;

    size_t measurementCount() const;

    const MeasurementSample* measurementAt(
        size_t index) const;

    size_t outputCount() const;

    const OutputSample* outputAt(
        size_t index) const;

    const MeasurementSample* find(
        const Measurement& measurement) const;

    const OutputSample* find(
        const Output& output) const;

    size_t alarmCount() const;

    const AlarmSample* alarmAt(size_t index) const;

    const AlarmSample* find(const LimitAlarm& alarm) const;

    uint32_t capturedAt() const;

    /** Heure du DS3231 ; valid est faux si elle est inconnue. */
    const ClockSample& clock() const;

    /** Vrai si une régulation a besoin de l'heure (programme en mode Auto). */
    bool clockRequired() const;

private:
    friend class ProcessControl;

    void clear(uint32_t now);

    bool add(const DigitalInput& input);

    bool add(
        const Measurement& measurement);

    bool add(
        const Output& output);

    bool add(const LimitAlarm& alarm);

    MeasurementSample measurementSamples[MAX_MEASUREMENTS] = {};
    DigitalInputSample digitalInputSamples[MAX_DIGITAL_INPUTS] = {};
    size_t digitalInputSampleCount = 0;
    OutputSample outputSamples[MAX_REGISTERED_OUTPUTS] = {};
    size_t measurementSampleCount = 0;
    size_t outputSampleCount = 0;
    AlarmSample alarmSamples[MAX_ALARMS] = {};
    size_t alarmSampleCount = 0;
    uint32_t captureTime = 0;
    ClockSample clockSample;
    bool clockNeeded = false;
};

#endif
