// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Hardware/AcquisitionSequencer.h>

#include <string>
#include <vector>

/*
 * Séquenceur d'acquisition : l'ISR est simulée par des appels directs à
 * onConversion(), loop() par poll(). Le double du matériel journalise ses
 * appels et vérifie qu'aucun n'arrive pendant que l'ISR est armée.
 */
namespace
{
    using Type = Sensor::Type;
    using Wiring = Sensor::Wiring;
    using Calls = std::vector<std::string>;

    constexpr uint16_t RTD_SETTLING = RTD_DISCARDED_CONVERSIONS;

    class FakeHardware final : public AcquisitionHardware
    {
    public:
        const AcquisitionSequencer* sequencer = nullptr;
        Calls calls;
        int32_t temperatureRaw = 0;
        int callsWhileArmed = 0;
        int restartsWithInterrupts = 0;
        bool interruptsDisabled = false;

        void routeInput(const Sensor::Settings&, uint8_t channel) override
        {
            record("route " + std::to_string(channel));
        }

        void disconnectInput(const Sensor::Settings&) override
        {
            record("disconnect");
        }

        void stopConversions() override { record("stop"); }
        void invertCurrents() override { record("invert"); }

        void selectExtendedGain(const Sensor::Settings&) override
        {
            record("extended gain");
        }

        void beginTemperature() override { record("temperature on"); }

        void endTemperature(int32_t raw) override
        {
            temperatureRaw = raw;
            record("temperature off");
        }

        uint32_t disableInterrupts() override
        {
            interruptsDisabled = true;
            return 0x5A;
        }

        void restoreInterrupts(uint32_t state) override
        {
            CHECK_TRUE(state == 0x5A);
            interruptsDisabled = false;
        }

        void restartConversions(bool continuous) override
        {
            if (!interruptsDisabled)
                restartsWithInterrupts++;

            record(continuous ? "restart continuous" : "restart single");
        }

        // Appels journalisés depuis le dernier take().
        Calls take()
        {
            Calls taken;
            taken.swap(calls);
            return taken;
        }

    private:
        void record(const std::string& call)
        {
            if (sequencer != nullptr && sequencer->isArmed())
                callsWhileArmed++;

            calls.push_back(call);
        }
    };

    struct Bench
    {
        FakeHardware hardware;
        AcquisitionSequencer sequencer{hardware};

        Bench() { hardware.sequencer = &sequencer; }

        // Conversions vues par l'ISR.
        void convert(int32_t value, int times = 1)
        {
            for (int i = 0; i < times; i++)
                sequencer.onConversion(value);
        }

        void checkHardwareDiscipline() const
        {
            CHECK_TRUE(hardware.callsWhileArmed == 0);
            CHECK_TRUE(hardware.restartsWithInterrupts == 0);
            CHECK_FALSE(hardware.interruptsDisabled);
        }
    };

    // Échantillons par série : samples × RTD_OVERSAMPLING pour une RTD.
    constexpr uint16_t SAMPLES = 2;
    constexpr int SERIES = SAMPLES * RTD_OVERSAMPLING;

    void testNominalCycle()
    {
        Bench bench;
        Sensor first;
        Sensor second;
        first.begin("first", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        second.begin("second", Type::Pt1000, Wiring::TwoWire, SAMPLES, 0);
        Sensor* inputs[] = {&first, &second};

        bench.sequencer.start(inputs, 2);
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"route 0", "restart continuous"}));
        CHECK_TRUE(bench.sequencer.isArmed());

        // Conversions d'établissement ignorées, puis la série.
        bench.convert(-500, RTD_SETTLING);
        bench.convert(1000, SERIES);
        CHECK_FALSE(bench.sequencer.isArmed());
        CHECK_TRUE(bench.hardware.take().empty());

        // L'ISR désarmée ignore toute conversion jusqu'à poll().
        bench.convert(9999, 3);

        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "disconnect", "route 1",
                          "restart continuous"}));
        CHECK_NEAR(first.readValue(), 1000.0, 1e-9);
        CHECK_TRUE(first.acquisitionStatus() == MeasurementStatus::Ok);

        bench.convert(-500, RTD_SETTLING);
        bench.convert(2000, SERIES);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "disconnect", "temperature on",
                          "restart single"}));
        CHECK_NEAR(second.readValue(), 2000.0, 1e-9);
        CHECK_FALSE(bench.sequencer.takeCycleCompleted());

        // Température de l'ADC : conversion unique, rien d'ignoré.
        bench.convert(3200);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"temperature off", "route 0",
                          "restart continuous"}));
        CHECK_TRUE(bench.hardware.temperatureRaw == 3200);
        CHECK_TRUE(bench.sequencer.takeCycleCompleted());
        CHECK_FALSE(bench.sequencer.takeCycleCompleted());
        CHECK_TRUE(bench.sequencer.isArmed());

        bench.checkHardwareDiscipline();
    }

    void testThreeWireChopping()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::ThreeWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.hardware.take();

        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES / 2);
        CHECK_FALSE(bench.sequencer.isArmed());

        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "invert", "restart continuous"}));

        // Nouvel établissement après l'inversion, puis seconde moitié.
        bench.convert(0, RTD_SETTLING);
        bench.convert(1010, SERIES / 2);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "disconnect", "temperature on",
                          "restart single"}));
        CHECK_NEAR(sensor.readValue(), 1005.0, 1e-9);

        bench.checkHardwareDiscipline();
    }

    void runRangeDiagnostic(int32_t extendedValue, MeasurementStatus expected)
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.hardware.take();

        // Saturée en étendue précise : conversion au gain large.
        bench.convert(0, RTD_SETTLING);
        bench.convert(32767, SERIES);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "extended gain", "restart continuous"}));

        bench.convert(0, RTD_SETTLING);
        bench.convert(extendedValue);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "disconnect", "temperature on",
                          "restart single"}));
        CHECK_TRUE(sensor.acquisitionStatus() == expected);

        bench.checkHardwareDiscipline();
    }

    void testRangeDiagnostic()
    {
        // Lisible au gain large : au-delà de l'étendue précise.
        runRangeDiagnostic(20000, MeasurementStatus::OverRange);

        // Encore saturée : entrée ouverte.
        runRangeDiagnostic(32767, MeasurementStatus::Open);
    }

    void testThermocoupleSettling()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("tc", Type::Tc, Wiring::TwoWire, 3, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.hardware.take();

        // Une seule conversion ignorée, puis la série sans suréchantillonnage.
        bench.convert(-1000);
        bench.convert(300, 3);
        CHECK_FALSE(bench.sequencer.isArmed());

        bench.sequencer.poll();
        CHECK_NEAR(sensor.readValue(), 300.0, 1e-9);

        bench.checkHardwareDiscipline();
    }

    void testStopAndRestart()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.convert(0, RTD_SETTLING + 10);
        bench.hardware.take();

        // Arrêt par le menu : plus aucune conversion ni étape.
        bench.sequencer.stop();
        CHECK_FALSE(bench.sequencer.isArmed());
        CHECK_FALSE(bench.sequencer.isRunning());

        bench.convert(1000, SERIES);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take().empty());
        CHECK_FALSE(bench.sequencer.takeCycleCompleted());

        // Un événement en attente est oublié par l'arrêt.
        sensor.reset();
        bench.sequencer.start(inputs, 1);
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);
        bench.sequencer.stop();
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"route 0", "restart continuous"}));

        bench.sequencer.start(inputs, 1);
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"route 0", "restart continuous"}));
        CHECK_TRUE(bench.sequencer.isRunning());

        bench.checkHardwareDiscipline();
    }

    void testWithoutInput()
    {
        Bench bench;

        bench.sequencer.start(nullptr, 0);
        CHECK_FALSE(bench.sequencer.isRunning());
        CHECK_FALSE(bench.sequencer.isArmed());

        bench.convert(1000);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take().empty());
    }
}

void runAcquisitionSequencerTests()
{
    TestHarness::run("acquisition : cycle de deux entrées", testNominalCycle);
    TestHarness::run("acquisition : 3 fils, inversion à mi-série",
        testThreeWireChopping);
    TestHarness::run("acquisition : diagnostic d'étendue", testRangeDiagnostic);
    TestHarness::run("acquisition : thermocouple", testThermocoupleSettling);
    TestHarness::run("acquisition : arrêt et reprise", testStopAndRestart);
    TestHarness::run("acquisition : sans entrée", testWithoutInput);
}
