// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/AcquisitionSequencer.h>

namespace
{
    constexpr int32_t ADC_POSITIVE_SATURATION = 32767;

    // Durée d'une conversion : RTD à 1000 SPS, thermocouple à 20 SPS.
    constexpr uint32_t RTD_CONVERSION_MS = 1;
    constexpr uint32_t TC_CONVERSION_MS = 50;

    /*
     * Délai d'une étape : deux fois sa durée normale, plus une seconde pour
     * une écriture en flash, qui coupe les interruptions.
     */
    constexpr uint32_t TIMEOUT_MARGIN_MS = 1000;

    uint32_t conversionMs(const Sensor::Settings& settings)
    {
        return settings.type == Sensor::Type::Tc
            ? TC_CONVERSION_MS
            : RTD_CONVERSION_MS;
    }
}

AcquisitionSequencer::AcquisitionSequencer(AcquisitionHardware& hardware)
    : hardware(hardware)
{
}

void AcquisitionSequencer::start(Sensor* const* inputs, uint8_t inputCount)
{
    stop();

    sensors = inputs;
    count = inputCount;
    channel = 0;
    cycleCompleted = false;
    report = CycleReport{};
    lastReport = CycleReport{};

    if (sensors == nullptr || count == 0)
        return;

    routeCurrent();
}

void AcquisitionSequencer::stop()
{
    armed.store(false, std::memory_order_release);
    event.store(Event::None, std::memory_order_release);
    step = Step::Stopped;
}

bool AcquisitionSequencer::takeCycleCompleted()
{
    const bool completed = cycleCompleted;
    cycleCompleted = false;
    return completed;
}

void AcquisitionSequencer::onConversion(int32_t value)
{
    if (!armed.load(std::memory_order_acquire))
        return;

    if (discard > 0)
    {
        discard--;
        return;
    }

    if (step != Step::Accumulating)
    {
        singleValue = value;
        disarm(Event::SingleDone);
        return;
    }

    Sensor& sensor = current();
    sensor.add(value);

    // 3 fils : les sources de courant sont inversées à mi-série pour
    // supprimer leur inégalité.
    if (sensor.isAccumulationHalfWay())
        disarm(Event::HalfWay);
    else if (sensor.isAccumulationDone())
        disarm(Event::SeriesDone);
}

void AcquisitionSequencer::poll()
{
    if (step == Step::Stopped)
        return;

    Event reached = event.exchange(Event::None, std::memory_order_acquire);

    if (reached != Event::None)
    {
        handle(reached);
        return;
    }

    if (hardware.milliseconds() - stepStartedAt < stepDurationMs)
        return;

    if (step == Step::Waiting)
    {
        rejectInput();
        return;
    }

    // Délai dépassé, ISR armée. Si elle a fini l'étape juste avant le
    // désarmement, l'événement l'emporte.
    armed.store(false, std::memory_order_release);
    reached = event.exchange(Event::None, std::memory_order_acquire);

    if (reached != Event::None)
        handle(reached);
    else
        handleTimeout();
}

void AcquisitionSequencer::handle(Event reached)
{
    switch (reached)
    {
    case Event::HalfWay:
        hardware.stopConversions();
        hardware.invertCurrents();
        arm(Step::Accumulating,
            settlingConversions(current().settings),
            true);
        break;

    case Event::SeriesDone:
        hardware.stopConversions();

        // Saturée en étendue précise : une conversion au gain de l'étendue
        // large dira si l'entrée est ouverte ou seulement au-delà.
        if (current().needsRangeDiagnostic())
        {
            hardware.selectExtendedGain(current().settings);
            arm(Step::Diagnosing,
                settlingConversions(current().settings),
                true);
        }
        else
        {
            finishInput();
        }
        break;

    case Event::SingleDone:
        if (step == Step::Diagnosing)
        {
            // Lisible au gain large : seulement au-delà de l'étendue
            // précise. Encore saturée : entrée ouverte.
            current().setRangeDiagnostic(
                singleValue < ADC_POSITIVE_SATURATION);
            hardware.stopConversions();
            finishInput();
        }
        else
        {
            hardware.endTemperature(singleValue);
            completeCycle();
        }
        break;

    case Event::None:
        break;
    }
}

void AcquisitionSequencer::handleTimeout()
{
    timeouts++;
    report.converterTimeout = true;
    hardware.resetConverter();

    if (step == Step::Temperature)
    {
        hardware.failTemperature();
        completeCycle();
        return;
    }

    rejectInput();
}

void AcquisitionSequencer::routeCurrent()
{
    if (hardware.routeInput(current().settings, channel))
    {
        arm(Step::Accumulating, settlingConversions(current().settings), true);
        return;
    }

    // Routage non prouvé : l'entrée garde sa durée normale, puis elle est
    // rejetée.
    report.routingFault = true;
    step = Step::Waiting;
    startTimer(inputDurationMs(current().settings));
}

void AcquisitionSequencer::finishInput()
{
    Sensor& sensor = current();

    // Routage encore en place : il a tenu pendant toute l'acquisition.
    if (hardware.confirmInput())
    {
        sensor.compute();
    }
    else
    {
        report.routingFault = true;
        sensor.reject();
    }

    hardware.disconnectInput(sensor.settings);
    nextInput();
}

void AcquisitionSequencer::rejectInput()
{
    current().reject();
    hardware.disconnectInput(current().settings);
    nextInput();
}

void AcquisitionSequencer::nextInput()
{
    channel++;

    if (channel < count)
    {
        routeCurrent();
        return;
    }

    // Toutes les entrées sont mesurées : la température de l'ADC (jonction
    // froide) termine le cycle.
    channel = 0;
    hardware.beginTemperature();
    arm(Step::Temperature, 0, false);
}

void AcquisitionSequencer::completeCycle()
{
    lastReport = report;
    report = CycleReport{};
    cycleCompleted = true;
    channel = 0;
    routeCurrent();
}

void AcquisitionSequencer::arm(
    Step nextStep,
    uint16_t conversionsToDiscard,
    bool continuous)
{
    step = nextStep;
    discard = conversionsToDiscard;

    switch (nextStep)
    {
    case Step::Accumulating:
        startTimer(
            2 * inputDurationMs(current().settings) + TIMEOUT_MARGIN_MS);
        break;

    case Step::Diagnosing:
        startTimer(
            2 * (conversionsToDiscard + 1u) * conversionMs(current().settings) +
            TIMEOUT_MARGIN_MS);
        break;

    default:
        // Température : une conversion au débit de la dernière entrée.
        startTimer(2 * TC_CONVERSION_MS + TIMEOUT_MARGIN_MS);
        break;
    }

    // Sans interruption entre la relance et l'armement : le front d'une
    // conversion faite entre les deux serait acquitté, donc perdu, et une
    // conversion unique ne reviendrait jamais.
    const uint32_t state = hardware.disableInterrupts();
    hardware.restartConversions(continuous);
    armed.store(true, std::memory_order_release);
    hardware.restoreInterrupts(state);
}

void AcquisitionSequencer::disarm(Event reached)
{
    armed.store(false, std::memory_order_release);
    event.store(reached, std::memory_order_release);
}

void AcquisitionSequencer::startTimer(uint32_t durationMs)
{
    stepStartedAt = hardware.milliseconds();
    stepDurationMs = durationMs;
}

uint16_t AcquisitionSequencer::settlingConversions(
    const Sensor::Settings& settings)
{
    return settings.type != Sensor::Type::Tc
        ? RTD_DISCARDED_CONVERSIONS
        : 1;
}

uint32_t AcquisitionSequencer::inputDurationMs(
    const Sensor::Settings& settings)
{
    const bool rtd = settings.type != Sensor::Type::Tc;
    const uint32_t samples = settings.samples > 0
        ? static_cast<uint32_t>(settings.samples)
        : 0;

    uint32_t conversions =
        settlingConversions(settings) +
        samples * (rtd ? RTD_OVERSAMPLING : 1u);

    // 3 fils : un second établissement après l'inversion des courants.
    if (rtd && settings.wiring == Sensor::Wiring::ThreeWire)
        conversions += settlingConversions(settings);

    return conversions * conversionMs(settings);
}
