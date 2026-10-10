// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/AcquisitionSequencer.h>

namespace
{
    constexpr int32_t ADC_POSITIVE_SATURATION = 32767;
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
    const Event reached =
        event.exchange(Event::None, std::memory_order_acquire);

    if (reached == Event::None || step == Step::Stopped)
        return;

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
            cycleCompleted = true;
            channel = 0;
            routeCurrent();
        }
        break;

    case Event::None:
        break;
    }
}

void AcquisitionSequencer::routeCurrent()
{
    hardware.routeInput(current().settings, channel);
    arm(Step::Accumulating, settlingConversions(current().settings), true);
}

void AcquisitionSequencer::finishInput()
{
    Sensor& sensor = current();
    sensor.compute();
    hardware.disconnectInput(sensor.settings);

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

void AcquisitionSequencer::arm(
    Step nextStep,
    uint16_t conversionsToDiscard,
    bool continuous)
{
    step = nextStep;
    discard = conversionsToDiscard;

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

uint16_t AcquisitionSequencer::settlingConversions(
    const Sensor::Settings& settings)
{
    return settings.type != Sensor::Type::Tc
        ? RTD_DISCARDED_CONVERSIONS
        : 1;
}
