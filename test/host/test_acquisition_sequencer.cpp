// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Hardware/AcquisitionSequencer.h>

#include <cmath>
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
        uint32_t now = 0;

        // Prochains routages refusés (routage non prouvé).
        int routeFailures = 0;
        bool routeHolds = true;

        bool routeInput(const Sensor::Settings&, uint8_t channel) override
        {
            record("route " + std::to_string(channel));

            if (routeFailures == 0)
                return true;

            routeFailures--;
            return false;
        }

        bool confirmInput() override
        {
            record("confirm");
            return routeHolds;
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

        void failTemperature() override { record("temperature failed"); }
        void resetConverter() override { record("reset converter"); }
        uint32_t milliseconds() override { return now; }

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
                   Calls({"stop", "confirm", "disconnect", "route 1",
                          "restart continuous"}));
        CHECK_NEAR(first.readValue(), 1000.0, 1e-9);
        CHECK_TRUE(first.acquisitionStatus() == MeasurementStatus::Ok);

        bench.convert(-500, RTD_SETTLING);
        bench.convert(2000, SERIES);
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "confirm", "disconnect", "temperature on",
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
                   Calls({"stop", "confirm", "disconnect", "temperature on",
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
                   Calls({"stop", "confirm", "disconnect", "temperature on",
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

    // PT100 4 fils, 2 échantillons : 15 + 100 conversions à 1 ms.
    constexpr uint32_t PT100_DURATION_MS = RTD_SETTLING + SERIES;

    void testRoutingFailureKeepsDuration()
    {
        Bench bench;
        Sensor first;
        Sensor second;
        first.begin("first", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        second.begin("second", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&first, &second};

        // Routage non prouvé : rien n'est lancé sur l'ADC.
        bench.hardware.routeFailures = 1;
        bench.sequencer.start(inputs, 2);
        CHECK_TRUE(bench.hardware.take() == Calls({"route 0"}));
        CHECK_FALSE(bench.sequencer.isArmed());

        // L'entrée garde sa durée normale avant d'être rejetée.
        bench.hardware.now = PT100_DURATION_MS - 1;
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take().empty());

        bench.hardware.now = PT100_DURATION_MS;
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"disconnect", "route 1", "restart continuous"}));
        CHECK_TRUE(first.acquisitionStatus() == MeasurementStatus::Invalid);
        CHECK_FALSE(std::isfinite(first.readValue()));

        // Le cycle continue : seconde entrée, température.
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);
        bench.sequencer.poll();
        bench.convert(3200);
        bench.sequencer.poll();
        CHECK_TRUE(bench.sequencer.takeCycleCompleted());
        CHECK_TRUE(bench.sequencer.lastCycle().routingFault);
        CHECK_FALSE(bench.sequencer.lastCycle().converterTimeout);
        CHECK_TRUE(second.acquisitionStatus() == MeasurementStatus::Ok);

        // Cycle suivant sans défaut.
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);
        bench.sequencer.poll();
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);
        bench.sequencer.poll();
        bench.convert(3200);
        bench.sequencer.poll();
        CHECK_TRUE(bench.sequencer.takeCycleCompleted());
        CHECK_FALSE(bench.sequencer.lastCycle().routingFault);
        CHECK_TRUE(first.acquisitionStatus() == MeasurementStatus::Ok);

        bench.checkHardwareDiscipline();
    }

    void testRoutingLostDuringAcquisition()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);

        // Routage disparu en fin d'acquisition : la série est rejetée.
        bench.hardware.routeHolds = false;
        bench.sequencer.poll();
        CHECK_TRUE(sensor.acquisitionStatus() == MeasurementStatus::Invalid);
        CHECK_FALSE(std::isfinite(sensor.readValue()));

        bench.convert(3200);
        bench.sequencer.poll();
        CHECK_TRUE(bench.sequencer.takeCycleCompleted());
        CHECK_TRUE(bench.sequencer.lastCycle().routingFault);

        bench.checkHardwareDiscipline();
    }

    void testConverterTimeout()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.hardware.take();

        // DRDY muet : deux fois la durée normale, plus une seconde.
        const uint32_t timeout = 2 * PT100_DURATION_MS + 1000;
        bench.hardware.now = timeout - 1;
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take().empty());
        CHECK_TRUE(bench.sequencer.isArmed());

        bench.hardware.now = timeout;
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"reset converter", "disconnect", "temperature on",
                          "restart single"}));
        CHECK_TRUE(sensor.acquisitionStatus() == MeasurementStatus::Invalid);
        CHECK_TRUE(bench.sequencer.converterTimeouts() == 1);

        // Température lue : le cycle se termine avec le défaut.
        bench.convert(3200);
        bench.sequencer.poll();
        CHECK_TRUE(bench.sequencer.takeCycleCompleted());
        CHECK_TRUE(bench.sequencer.lastCycle().converterTimeout);
        CHECK_FALSE(bench.sequencer.lastCycle().routingFault);

        bench.checkHardwareDiscipline();
    }

    void testTemperatureTimeout()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);
        bench.sequencer.poll();
        bench.hardware.take();

        // Conversion de température jamais arrivée.
        bench.hardware.now = 2 * 50 + 1000;
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"reset converter", "temperature failed", "route 0",
                          "restart continuous"}));
        CHECK_TRUE(bench.sequencer.takeCycleCompleted());
        CHECK_TRUE(bench.sequencer.lastCycle().converterTimeout);

        // La mesure de l'entrée, faite avant, reste valable.
        CHECK_TRUE(sensor.acquisitionStatus() == MeasurementStatus::Ok);

        bench.checkHardwareDiscipline();
    }

    void testEventBeatsTimeout()
    {
        Bench bench;
        Sensor sensor;
        sensor.begin("sensor", Type::Pt100, Wiring::FourWire, SAMPLES, 0);
        Sensor* inputs[] = {&sensor};

        bench.sequencer.start(inputs, 1);
        bench.hardware.take();

        // Série terminée, mais loop() n'est revenue qu'après le délai
        // (sauvegarde en flash, par exemple) : pas de défaut.
        bench.convert(0, RTD_SETTLING);
        bench.convert(1000, SERIES);
        bench.hardware.now = 10000;
        bench.sequencer.poll();
        CHECK_TRUE(bench.hardware.take() ==
                   Calls({"stop", "confirm", "disconnect", "temperature on",
                          "restart single"}));
        CHECK_TRUE(sensor.acquisitionStatus() == MeasurementStatus::Ok);
        CHECK_TRUE(bench.sequencer.converterTimeouts() == 0);

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
    TestHarness::run("acquisition : routage refusé, durée normale gardée",
        testRoutingFailureKeepsDuration);
    TestHarness::run("acquisition : routage perdu pendant la série",
        testRoutingLostDuringAcquisition);
    TestHarness::run("acquisition : ADC muet", testConverterTimeout);
    TestHarness::run("acquisition : température ADC muette",
        testTemperatureTimeout);
    TestHarness::run("acquisition : série finie après le délai",
        testEventBeatsTimeout);
}
