#include "TestHarness.h"

#include <EventLog.h>
#include <Hardware/pinout.h>
#include <Inputs/DigitalInput.h>
#include <Measurements/Temperature/Temperature.h>
#include <Outputs/Actuator.h>
#include <ProcessControl.h>
#include <ProcessLogic.h>
#include <Regulator/Alarm.h>
#include <Regulator/LogicCommand.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>
#include <string>

namespace
{
    using Status = MeasurementStatus;
    using Operation = LogicCommand::Operation;

    constexpr uint8_t PIN_1 = Board::Rp2040::DIGITAL_INPUT_1;

    // Ordre des appels pendant un cycle : R régulateur, L glue, A actionneur.
    std::string sequence;

    class ControlledTemperature final : public Temperature
    {
    public:
        ControlledTemperature()
        {
            begin("temperature");
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

    // Régulateur dont la commande est fixée par le test.
    class TestRegulator final : public Regulator
    {
    public:
        bool valid = true;

        void update(uint32_t now) override
        {
            (void)now;
            sequence += 'R';

            if (valid)
                writeCommand(1.0);
            else
                invalidateCommand();
        }
    };

    class RecordingActuator final : public Actuator
    {
    public:
        bool seenValid = false;
        double_t seen = -1.0;

        void update(uint32_t now) override
        {
            (void)now;
            sequence += 'A';
            seenValid = regulator->isCommandValid();
            seen = regulator->readCommand();
        }
    };

    class TestLogic final : public ProcessLogic
    {
    public:
        enum class Write
        {
            Value,
            Invalidate,
            Nothing
        };

        LogicCommand* command = nullptr;
        Write write = Write::Value;
        double_t value = 1.0;

        // Régulateur lu par la glue, et sa validité au moment de l'appel.
        const Regulator* watched = nullptr;
        bool watchedValid = false;

        unsigned resumes = 0;

        void processLogic(uint32_t now) override
        {
            (void)now;
            sequence += 'L';

            if (watched != nullptr)
                watchedValid = watched->isCommandValid();

            if (command == nullptr)
                return;

            if (write == Write::Value)
                command->set(value);
            else if (write == Write::Invalidate)
                command->invalidate();
        }

        void resumeLogic(uint32_t now) override
        {
            (void)now;
            resumes++;
        }
    };

    class LockingAlarm final : public Alarm
    {
    public:
        bool lock = false;

        bool isEnabled() const override
        {
            return true;
        }

        bool locksOutputs() const override
        {
            return lock;
        }

        void update(uint32_t now) override
        {
            (void)now;
        }
    };

    // Une commande de glue reliée à un actionneur qui enregistre ce qu'il lit.
    struct Bench
    {
        ProcessControl process;
        LogicCommand command;
        TestLogic logic;
        RecordingActuator actuator;

        Bench()
        {
            command.begin("commande", "Commande");
            logic.command = &command;
            actuator.begin("actionneur", command);
            process.add(command);
            process.add(actuator);
            process.setLogic(logic);
        }

        void cycle(uint32_t now)
        {
            process.updateMeasurementsAndRegulators(now);
        }

        size_t events() const
        {
            return process.eventLog().count();
        }
    };

    void testCallOrder()
    {
        Bench bench;
        TestRegulator regulator;
        regulator.begin("regulateur");
        bench.process.add(regulator);
        bench.logic.watched = &regulator;

        sequence.clear();
        bench.cycle(1000);

        // Régulateurs, puis glue, puis actionneurs.
        CHECK_TRUE(sequence == "RLA");

        // La glue voit le régulateur à jour...
        CHECK_TRUE(bench.logic.watchedValid);

        // ... et sa commande sert aux actionneurs dès ce cycle.
        CHECK_TRUE(bench.actuator.seenValid);
        CHECK_NEAR(bench.actuator.seen, 1.0, 0.0);
    }

    void testWrittenValues()
    {
        Bench bench;

        bench.logic.value = 0.25;
        bench.cycle(1000);
        CHECK_TRUE(bench.command.isCommandValid());
        CHECK_NEAR(bench.command.readCommand(), 0.25, 1e-12);

        // Commande bornée à 0..1.
        bench.logic.value = 1.5;
        bench.cycle(2000);
        CHECK_NEAR(bench.command.readCommand(), 1.0, 0.0);

        bench.logic.value = -0.5;
        bench.cycle(3000);
        CHECK_NEAR(bench.command.readCommand(), 0.0, 0.0);

        const size_t events = bench.events();

        // Valeur non finie ou invalidate() : état sûr, sans oubli journalisé.
        bench.logic.value = NAN;
        bench.cycle(4000);
        CHECK_FALSE(bench.command.isCommandValid());

        bench.logic.write = TestLogic::Write::Invalidate;
        bench.cycle(5000);
        CHECK_FALSE(bench.command.isCommandValid());
        CHECK_FALSE(bench.actuator.seenValid);

        CHECK_TRUE(bench.events() == events);
    }

    void testMissingWrite()
    {
        Bench bench;
        bench.cycle(1000);
        CHECK_TRUE(bench.command.isCommandValid());

        const size_t events = bench.events();

        // La glue n'écrit pas : état sûr dès ce cycle, et un événement.
        bench.logic.write = TestLogic::Write::Nothing;
        bench.cycle(2000);
        CHECK_FALSE(bench.command.isCommandValid());
        CHECK_FALSE(bench.actuator.seenValid);
        CHECK_TRUE(bench.events() == events + 1);

        const EventEntry* entry = bench.process.eventLog().at(0);
        CHECK_TRUE(entry != nullptr);
        CHECK_TRUE(std::strcmp(entry->text, "Commande : non écrite") == 0);
        CHECK_TRUE(entry->kind == EventKind::Fault);
        CHECK_TRUE(entry->important);

        // Un seul événement tant que l'oubli dure.
        bench.cycle(3000);
        CHECK_TRUE(bench.events() == events + 1);
        CHECK_TRUE(bench.process.eventLog().at(0)->repeats == 1);

        bench.logic.write = TestLogic::Write::Value;
        bench.cycle(4000);
        CHECK_TRUE(bench.command.isCommandValid());

        // Nouvel oubli, nouvel événement.
        bench.logic.write = TestLogic::Write::Nothing;
        bench.cycle(100000);
        CHECK_TRUE(bench.events() == events + 2);
    }

    void testMeasurementDependency()
    {
        Bench bench;
        ControlledTemperature temperature;
        temperature.set(20.0);
        CHECK_TRUE(bench.command.dependsOn(temperature));

        bench.cycle(1000);
        CHECK_TRUE(bench.command.isCommandValid());

        // Sonde en défaut : état sûr, quoi que la glue écrive.
        temperature.fail(Status::Open);
        bench.cycle(2000);
        CHECK_FALSE(bench.command.isCommandValid());
        CHECK_FALSE(bench.command.isInFallback());
        CHECK_FALSE(bench.actuator.seenValid);

        // Pendant le défaut, une glue qui n'écrit rien n'est pas un oubli.
        const size_t events = bench.events();
        bench.logic.write = TestLogic::Write::Nothing;
        bench.cycle(3000);
        CHECK_TRUE(bench.events() == events);

        // Retour de la mesure.
        temperature.set(20.0);
        bench.logic.write = TestLogic::Write::Value;
        bench.cycle(4000);
        CHECK_TRUE(bench.command.isCommandValid());

        // Maintien limité de la commande d'avant le défaut.
        bench.command.faultSettings.action = Regulator::FaultAction::Hold;
        bench.command.faultSettings.holdTime = 10;
        temperature.fail(Status::Short);
        bench.cycle(5000);
        CHECK_TRUE(bench.command.isCommandValid());
        CHECK_TRUE(bench.command.isInFallback());
        CHECK_NEAR(bench.command.readCommand(), 1.0, 0.0);

        bench.cycle(14999);
        CHECK_TRUE(bench.command.isInFallback());

        bench.cycle(15000);
        CHECK_FALSE(bench.command.isCommandValid());
    }

    void testNotReadyNeverHolds()
    {
        Bench bench;
        ControlledTemperature temperature;
        bench.command.dependsOn(temperature);
        bench.command.faultSettings.action = Regulator::FaultAction::Hold;

        temperature.fail(Status::NotReady);
        bench.cycle(1000);
        CHECK_FALSE(bench.command.isCommandValid());

        // Valeur non finie avec un état Ok : défaut aussi.
        temperature.set(NAN);
        bench.cycle(2000);
        CHECK_FALSE(bench.command.isCommandValid());
    }

    void testInputAndBlockDependencies()
    {
        Bench bench;
        DigitalInput input;
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.begin("entree", PIN_1);
        CHECK_TRUE(bench.command.dependsOn(input));

        TestRegulator block;
        block.begin("bloc");
        bench.process.add(block);
        CHECK_TRUE(bench.command.dependsOn(block));

        // Entrée jamais lue : invalide.
        bench.cycle(1000);
        CHECK_FALSE(bench.command.isCommandValid());

        input.poll(1500);
        bench.cycle(2000);
        CHECK_TRUE(bench.command.isCommandValid());

        // Bloc dont la commande est invalide.
        block.valid = false;
        bench.cycle(3000);
        CHECK_FALSE(bench.command.isCommandValid());

        // Quatre dépendances au plus.
        ControlledTemperature extra;
        CHECK_TRUE(bench.command.dependsOn(extra));
        CHECK_TRUE(bench.command.dependsOn(extra));
        CHECK_FALSE(bench.command.dependsOn(extra));
    }

    void testManualMode()
    {
        Bench bench;
        ControlledTemperature temperature;
        temperature.fail(Status::Open);
        bench.command.dependsOn(temperature);
        bench.logic.write = TestLogic::Write::Nothing;

        const size_t events = bench.events();

        // En manuel, ni la glue ni le défaut de sonde n'agissent.
        bench.command.settings.operation = Operation::ForcedOn;
        bench.cycle(1000);
        CHECK_FALSE(bench.command.isAutomatic());
        CHECK_TRUE(bench.command.isCommandValid());
        CHECK_NEAR(bench.command.readCommand(), 1.0, 0.0);
        CHECK_TRUE(bench.actuator.seenValid);

        bench.command.settings.operation = Operation::ForcedOff;
        bench.cycle(2000);
        CHECK_TRUE(bench.command.isCommandValid());
        CHECK_NEAR(bench.command.readCommand(), 0.0, 0.0);
        CHECK_TRUE(bench.events() == events);

        // Retour en Auto : le défaut reprend la main.
        bench.command.settings.operation = Operation::Auto;
        bench.cycle(3000);
        CHECK_TRUE(bench.command.isAutomatic());
        CHECK_FALSE(bench.command.isCommandValid());
    }

    void testManualModeRemoved()
    {
        Bench bench;
        bench.command.disableManualMode();
        bench.logic.value = 0.0;

        // Une valeur forcée (restaurée, par exemple) reste sans effet.
        bench.command.settings.operation = Operation::ForcedOn;
        bench.cycle(1000);
        CHECK_TRUE(bench.command.isAutomatic());
        CHECK_NEAR(bench.command.readCommand(), 0.0, 0.0);
    }

    void testParameters()
    {
        Parameter storage[8];
        ParameterList list;

        // Commande avec dépendance : « Commande » (live, non sauvegardée) et
        // réglage du défaut.
        ControlledTemperature temperature;
        LogicCommand pump;
        pump.begin("pompe", "Pompe");
        pump.dependsOn(temperature);

        list.begin(storage, 8);
        pump.registerParameters(list);
        CHECK_FALSE(list.hasError());

        const Parameter* operation = list.find("pompe", "operation");
        CHECK_TRUE(operation != nullptr);
        CHECK_TRUE(operation != nullptr && operation->live);
        CHECK_TRUE(operation != nullptr && !operation->persistent);
        CHECK_TRUE(list.find("pompe", "fault_action") != nullptr);

        // Sans dépendance : pas de réglage du défaut.
        LogicCommand fan;
        fan.begin("ventilateur", "Ventilateur");
        list.begin(storage, 8);
        fan.registerParameters(list);
        CHECK_TRUE(list.find("ventilateur", "operation") != nullptr);
        CHECK_TRUE(list.find("ventilateur", "fault_action") == nullptr);

        // Mode manuel retiré, action sur défaut verrouillée : aucun réglage.
        LogicCommand heater;
        heater.begin("resistance", "Resistance");
        heater.dependsOn(temperature);
        heater.disableManualMode();
        heater.lockFaultAction(Regulator::FaultAction::SafeState);
        list.begin(storage, 8);
        heater.registerParameters(list);
        CHECK_TRUE(list.count() == 0);
    }

    void testResume()
    {
        Bench bench;
        bench.cycle(1000);
        CHECK_TRUE(bench.command.isCommandValid());

        const size_t events = bench.events();

        // Reprise (menu) : commande invalide jusqu'à la prochaine écriture,
        // et la glue est prévenue.
        bench.process.resume(2000);
        CHECK_FALSE(bench.command.isCommandValid());
        CHECK_TRUE(bench.logic.resumes == 1);

        bench.cycle(3000);
        CHECK_TRUE(bench.command.isCommandValid());
        CHECK_TRUE(bench.events() == events);
    }

    void testInterlock()
    {
        Bench bench;
        LockingAlarm alarm;
        alarm.begin("alarme");
        bench.command.setInterlock(alarm);

        bench.cycle(1000);
        CHECK_TRUE(bench.command.isCommandValid());

        alarm.lock = true;
        bench.cycle(2000);
        CHECK_FALSE(bench.command.isCommandValid());
        CHECK_FALSE(bench.actuator.seenValid);
    }

    void testWithoutLogic()
    {
        // Commande enregistrée sans glue : jamais écrite, donc état sûr.
        ProcessControl process;
        LogicCommand command;
        command.begin("commande", "Commande");
        process.add(command);

        process.updateMeasurementsAndRegulators(1000);
        CHECK_FALSE(command.isCommandValid());
        CHECK_TRUE(process.eventLog().count() == 1);
    }
}

void runLogicTests()
{
    TestHarness::run("glue : ordre d'appel", testCallOrder);
    TestHarness::run("glue : valeurs écrites", testWrittenValues);
    TestHarness::run("glue : commande non écrite", testMissingWrite);
    TestHarness::run("glue : dépendance à une mesure", testMeasurementDependency);
    TestHarness::run("glue : mesure pas prête", testNotReadyNeverHolds);
    TestHarness::run("glue : dépendances entrée et bloc", testInputAndBlockDependencies);
    TestHarness::run("glue : mode manuel", testManualMode);
    TestHarness::run("glue : mode manuel retiré", testManualModeRemoved);
    TestHarness::run("glue : paramètres", testParameters);
    TestHarness::run("glue : reprise", testResume);
    TestHarness::run("glue : verrouillage par alarme", testInterlock);
    TestHarness::run("glue : commande sans glue", testWithoutLogic);
}
