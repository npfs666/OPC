// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <Measurements/Temperature/Temperature.h>
#include <Regulator/PID.h>
#include <Regulator/SetpointProgram.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    using Status = MeasurementStatus;
    using State = SetpointProgram::State;

    constexpr uint32_t MINUTE = 60000UL;

    class ControlledTemperature final : public Temperature
    {
    public:
        explicit ControlledTemperature(const char* name)
        {
            begin(name);
        }

        void update() override
        {
        }

        void set(double_t value)
        {
            setValue(value);
            setStatus(Status::Ok);
        }

        void fail(Status status)
        {
            setValue(NAN);
            setStatus(status);
        }
    };

    struct Bench
    {
        ControlledTemperature temperature{"four"};
        SetpointProgram program;
        uint32_t now = 0;
        // La mesure suit la consigne : four idéal.
        bool tracking = false;

        explicit Bench(uint8_t programCount = 1)
        {
            program.begin("prog", "Cuisson", temperature, programCount);
            temperature.set(20.0);
            program.update(now);
        }

        SetpointProgram::Segment& segment(uint8_t index, uint8_t programIndex = 0)
        {
            return program.settings.programs[programIndex].segments[index];
        }

        void setSegment(
            uint8_t index,
            double_t rate,
            double_t target,
            uint32_t soak,
            uint8_t programIndex = 0)
        {
            segment(index, programIndex) = {rate, target, soak};
        }

        // Cycles de 1 s pendant la durée donnée.
        void advance(uint32_t ms)
        {
            for (uint32_t t = 0; t < ms; t += 1000)
            {
                double_t value = 0.0;

                if (tracking && program.readSetpoint(value))
                    temperature.set(value);

                now += 1000;
                program.update(now);
            }
        }

        double_t setpoint()
        {
            double_t value = NAN;
            CHECK_TRUE(program.readSetpoint(value));
            return value;
        }
    };

    void testIdleAndStart()
    {
        Bench bench;
        bench.setSegment(0, 120.0, 100.0, 10);

        // Hors cuisson : arrêt commandé, pas de consigne.
        bench.advance(1000);
        double_t value = 0.0;
        CHECK_TRUE(bench.program.state() == State::Idle);
        CHECK_TRUE(bench.program.isCommandValid());
        CHECK_NEAR(bench.program.readCommand(), 0.0, 1e-12);
        CHECK_FALSE(bench.program.readSetpoint(value));
        CHECK_TRUE(bench.program.runningProgram() == 0);

        // Mesure invalide : pas de départ.
        bench.temperature.fail(Status::Open);
        CHECK_FALSE(bench.program.start());
        CHECK_TRUE(bench.program.state() == State::Idle);

        // Programme inexistant : pas de départ.
        bench.temperature.set(20.0);
        bench.program.settings.selected = 2;
        CHECK_FALSE(bench.program.start());

        bench.program.settings.selected = 1;
        CHECK_TRUE(bench.program.start());
        CHECK_TRUE(bench.program.isRunning());

        // Départ depuis la mesure.
        bench.advance(1000);
        CHECK_TRUE(bench.program.state() == State::Ramp);
        CHECK_NEAR(bench.program.readCommand(), 1.0, 1e-12);
        CHECK_NEAR(bench.setpoint(), 20.0, 1e-9);
        CHECK_TRUE(bench.program.runningProgram() == 1);
        CHECK_TRUE(bench.program.segment() == 1);
        CHECK_TRUE(bench.program.segmentCount() == 1);
    }

    void testRampAndSoak()
    {
        Bench bench;
        bench.setSegment(0, 120.0, 100.0, 10);
        bench.program.start();
        bench.advance(1000);

        // 120 °C/h : 2 °C par minute.
        bench.advance(MINUTE);
        CHECK_NEAR(bench.setpoint(), 22.0, 1e-6);

        // 40 min de rampe en tout + 10 min de palier.
        CHECK_TRUE(bench.program.remainingSeconds() == 49 * 60);

        bench.advance(39 * MINUTE);
        CHECK_TRUE(bench.program.state() == State::Soak);
        CHECK_NEAR(bench.setpoint(), 100.0, 1e-9);
        CHECK_TRUE(bench.program.soakRemainingSeconds() == 10 * 60);

        double_t target = 0.0;
        CHECK_TRUE(bench.program.readSegmentTarget(target));
        CHECK_NEAR(target, 100.0, 1e-9);

        bench.advance(9 * MINUTE);
        CHECK_TRUE(bench.program.state() == State::Soak);
        CHECK_TRUE(bench.program.soakRemainingSeconds() == 60);

        // Fin : arrêt commandé, programme terminé.
        bench.advance(MINUTE);
        CHECK_TRUE(bench.program.state() == State::Finished);
        CHECK_FALSE(bench.program.isRunning());
        CHECK_TRUE(bench.program.isCommandValid());
        CHECK_NEAR(bench.program.readCommand(), 0.0, 1e-12);
        CHECK_TRUE(bench.program.runningProgram() == 1);
        CHECK_TRUE(bench.program.elapsedSeconds() == 50 * 60);
    }

    void testHoldback()
    {
        Bench bench;
        bench.setSegment(0, 120.0, 100.0, 10);
        bench.program.settings.holdback = 5.0;
        bench.program.start();
        bench.advance(1000);

        // Le four reste à 20 °C : la rampe attend à 5 °C d'écart.
        bench.advance(30 * MINUTE);
        CHECK_TRUE(bench.program.isHeldBack());
        CHECK_NEAR(bench.setpoint(), 25.0, 0.05);

        // Le four rattrape : la rampe repart.
        bench.temperature.set(25.0);
        bench.advance(MINUTE);
        CHECK_FALSE(bench.program.isHeldBack());
        CHECK_NEAR(bench.setpoint(), 27.0, 0.1);

        // Palier : le temps ne compte qu'à l'écart maxi près.
        bench.tracking = true;
        bench.advance(37 * MINUTE);
        bench.tracking = false;
        CHECK_TRUE(bench.program.state() == State::Soak);

        const uint32_t remaining = bench.program.soakRemainingSeconds();
        CHECK_TRUE(remaining > 0 && remaining < 10 * 60);

        bench.temperature.set(90.0);
        bench.advance(30 * MINUTE);
        CHECK_TRUE(bench.program.isHeldBack());
        CHECK_TRUE(bench.program.soakRemainingSeconds() == remaining);

        bench.temperature.set(98.0);
        bench.advance((remaining - 1) * 1000);
        CHECK_TRUE(bench.program.state() == State::Soak);
        bench.advance(2000);
        CHECK_TRUE(bench.program.state() == State::Finished);
    }

    void testFullPowerAndCooling()
    {
        Bench bench;
        bench.program.settings.programs[0].segmentCount = 3;
        // Pleine puissance jusqu'à 600, refroidissement libre jusqu'à 500,
        // descente contrôlée à 60 °C/h jusqu'à 450.
        bench.setSegment(0, 0.0, 600.0, 0);
        bench.setSegment(1, 0.0, 500.0, 5);
        bench.setSegment(2, 60.0, 450.0, 0);
        bench.program.start();
        bench.advance(1000);

        // Consigne à la cible tout de suite ; la rampe dure tant que le four
        // n'y est pas.
        CHECK_NEAR(bench.setpoint(), 600.0, 1e-9);
        CHECK_FALSE(bench.program.isCooling());
        bench.advance(60 * MINUTE);
        CHECK_TRUE(bench.program.state() == State::Ramp);
        CHECK_FALSE(bench.program.isHeldBack());

        // Palier nul : segment suivant au cycle d'après.
        bench.temperature.set(600.0);
        bench.advance(3000);
        CHECK_TRUE(bench.program.segment() == 2);
        CHECK_NEAR(bench.setpoint(), 500.0, 1e-9);
        CHECK_TRUE(bench.program.isCooling());

        // Refroidissement libre : palier quand le four est descendu.
        bench.advance(10 * MINUTE);
        CHECK_TRUE(bench.program.state() == State::Ramp);
        bench.temperature.set(499.0);
        bench.advance(1000);
        CHECK_TRUE(bench.program.state() == State::Soak);
        bench.advance(5 * MINUTE);
        CHECK_TRUE(bench.program.segment() == 3);

        // Descente contrôlée : 1 °C par minute.
        bench.advance(10 * MINUTE);
        CHECK_NEAR(bench.setpoint(), 490.0, 1e-6);
    }

    void testDelaySkipAndEnd()
    {
        Bench bench;
        bench.program.settings.programs[0].segmentCount = 2;
        bench.setSegment(0, 60.0, 50.0, 30);
        bench.setSegment(1, 60.0, 80.0, 0);
        bench.program.settings.startDelay = 30;
        bench.program.settings.end = SetpointProgram::End::Hold;
        bench.program.start();

        // Départ différé : rien ne chauffe.
        bench.advance(MINUTE);
        double_t value = 0.0;
        CHECK_TRUE(bench.program.state() == State::Delayed);
        CHECK_FALSE(bench.program.readSetpoint(value));
        CHECK_NEAR(bench.program.readCommand(), 0.0, 1e-12);
        CHECK_TRUE(bench.program.delayRemainingSeconds() == 29 * 60);

        bench.advance(29 * MINUTE);
        CHECK_TRUE(bench.program.state() == State::Ramp);
        CHECK_TRUE(bench.program.elapsedSeconds() == 0);

        // Estimation : 30 min + 30 min de palier + 30 min.
        CHECK_TRUE(bench.program.remainingSeconds() == 90 * 60);

        // Segment suivant : la rampe repart de la consigne du moment.
        bench.advance(10 * MINUTE);
        CHECK_TRUE(bench.program.skipSegment());
        CHECK_TRUE(bench.program.segment() == 2);
        CHECK_NEAR(bench.setpoint(), 30.0, 1e-6);

        // Fin en maintien : consigne gardée, commande 1, sans fin.
        bench.advance(50 * MINUTE + 1000);
        CHECK_TRUE(bench.program.state() == State::Hold);
        CHECK_TRUE(bench.program.isRunning());
        CHECK_NEAR(bench.setpoint(), 80.0, 1e-9);
        CHECK_NEAR(bench.program.readCommand(), 1.0, 1e-12);
        CHECK_FALSE(bench.program.skipSegment());

        bench.program.stop();
        bench.advance(1000);
        CHECK_TRUE(bench.program.state() == State::Idle);
        CHECK_FALSE(bench.program.readSetpoint(value));

        // Segment suivant pendant le départ différé : départ immédiat.
        bench.program.start();
        bench.advance(1000);
        CHECK_TRUE(bench.program.skipSegment());
        bench.advance(1000);
        CHECK_TRUE(bench.program.state() == State::Ramp);
    }

    void testFreezes()
    {
        Bench bench;
        bench.setSegment(0, 60.0, 100.0, 0);
        bench.program.start();
        bench.advance(1000);
        bench.advance(10 * MINUTE);
        CHECK_NEAR(bench.setpoint(), 30.0, 1e-6);

        // Pause de 10 min (menu, timeout) : le programme ne bouge pas.
        bench.now += 10 * MINUTE;
        bench.program.resume(bench.now);
        bench.advance(1000);
        CHECK_NEAR(bench.setpoint(), 30.0 + 1.0 / 60.0, 1e-6);

        // Mesure invalide : programme figé, consigne et commande gardées.
        bench.temperature.fail(Status::Open);
        bench.advance(10 * MINUTE);
        CHECK_NEAR(bench.setpoint(), 30.0 + 1.0 / 60.0, 1e-6);
        CHECK_NEAR(bench.program.readCommand(), 1.0, 1e-12);

        // Inhibition : commande 0, pas de consigne, programme figé.
        bench.temperature.set(30.0);
        bench.program.inhibit(true);
        bench.advance(10 * MINUTE);
        double_t value = 0.0;
        CHECK_TRUE(bench.program.isCommandValid());
        CHECK_NEAR(bench.program.readCommand(), 0.0, 1e-12);
        CHECK_FALSE(bench.program.readSetpoint(value));

        bench.program.inhibit(false);
        bench.advance(1000);
        CHECK_NEAR(bench.setpoint(), 30.0 + 2.0 / 60.0, 1e-6);

        // Segments retirés au menu pendant la cuisson : fin du programme.
        bench.program.settings.programs[0].segmentCount = 2;
        bench.setSegment(1, 60.0, 200.0, 0);
        bench.program.skipSegment();
        bench.advance(1000);
        CHECK_TRUE(bench.program.segment() == 2);
        bench.program.settings.programs[0].segmentCount = 1;
        bench.advance(1000);
        CHECK_TRUE(bench.program.state() == State::Finished);
        CHECK_TRUE(bench.program.segment() == 1);
    }

    void testPIDFollows()
    {
        Bench bench;
        bench.setSegment(0, 60.0, 100.0, 0);

        PID pid;
        pid.begin("pid", "PID", bench.temperature);
        pid.followSetpoint(bench.program);
        pid.setTunings(0.05, 600.0, 0.0);

        auto cycle = [&](uint32_t seconds)
        {
            for (uint32_t i = 0; i < seconds; i++)
            {
                bench.now += 1000;
                bench.program.update(bench.now);
                pid.update(bench.now);
            }
        };

        // Hors cuisson : PID à l'arrêt commandé, pas en défaut.
        cycle(5);
        double_t value = 0.0;
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 1e-12);
        CHECK_FALSE(pid.readSetpoint(value));

        // En cuisson : le PID suit la consigne du programme et chauffe.
        bench.program.start();
        cycle(600);
        CHECK_TRUE(pid.readSetpoint(value));
        CHECK_NEAR(value, bench.setpoint(), 1e-9);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_TRUE(pid.readCommand() > 0.0);

        // Arrêt : retour à l'arrêt commandé.
        bench.program.stop();
        cycle(2);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 1e-12);
    }

    void testParameters()
    {
        Bench bench(3);
        bench.program.setLimits(0.0, 1300.0, 999.0);
        bench.setSegment(0, 100.0, 950.0, 15, 2);

        Parameter storage[128];
        ParameterList list;
        list.begin(storage, 128);
        bench.program.registerParameters(list);

        CHECK_FALSE(list.hasError());
        // 4 généraux + 3 × (1 + 8 × 3).
        CHECK_TRUE(list.count() == 4 + 3 * 25);

        const Parameter* program = list.find("prog", "program");
        CHECK_TRUE(program != nullptr && program->live);
        CHECK_TRUE(program != nullptr &&
                   program->data.integer.maximum == 3);

        const Parameter* count = list.find("prog.p3", "segments");
        CHECK_TRUE(count != nullptr && count->live);
        CHECK_TRUE(count != nullptr &&
                   std::strcmp(count->parentOwnerKey, "prog") == 0);

        const Parameter* target = list.find("prog.p3.s1", "target");
        CHECK_TRUE(target != nullptr && target->live);
        CHECK_TRUE(target != nullptr &&
                   std::strcmp(target->parentOwnerKey, "prog.p3") == 0);
        CHECK_NEAR(*target->value.number, 950.0, 1e-9);
        CHECK_NEAR(target->data.number.maximum, 1300.0, 1e-9);

        CHECK_TRUE(list.find("prog.p3.s8", "soak") != nullptr);
        CHECK_TRUE(list.find("prog.p4.s1", "target") == nullptr);
    }
}

void runSetpointProgramTests()
{
    TestHarness::run("Programme : arrêt et départ", testIdleAndStart);
    TestHarness::run("Programme : rampe et palier", testRampAndSoak);
    TestHarness::run("Programme : écart maxi", testHoldback);
    TestHarness::run("Programme : pleine puissance et descente", testFullPowerAndCooling);
    TestHarness::run("Programme : départ différé, saut, maintien", testDelaySkipAndEnd);
    TestHarness::run("Programme : pause, défaut, inhibition", testFreezes);
    TestHarness::run("Programme : PID suiveur", testPIDFollows);
    TestHarness::run("Programme : paramètres", testParameters);
}
