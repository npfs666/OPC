#include "TestHarness.h"

#include <EventLog.h>
#include <Hardware/pinout.h>
#include <Inputs/DigitalInput.h>
#include <ProcessControl.h>
#include <ProcessLogic.h>
#include <Regulator/ConditionAlarm.h>
#include <hmi/ParameterList.h>

#include <cstdint>
#include <cstring>

namespace
{
    constexpr uint8_t PIN_1 = Board::Rp2040::DIGITAL_INPUT_1;

    // Porte : entrée 1 active (HIGH) quand la porte est ouverte.
    bool doorAlarmAt(
        ConditionAlarm& alarm,
        DigitalInput& door,
        bool open,
        uint32_t now)
    {
        FakeDigitalIO::levels[PIN_1] = open ? HIGH : LOW;
        door.poll(now);
        alarm.update(now);
        return alarm.isActive();
    }

    void testDoorInput()
    {
        DigitalInput door;
        door.begin("porte", "Porte", PIN_1);

        ConditionAlarm alarm;
        alarm.begin("alarme_porte", "Porte ouverte", door);
        alarm.settings.enabled = true;
        alarm.settings.delay = 300;

        // Porte ouverte à 1 s : alarme après 5 min.
        CHECK_FALSE(doorAlarmAt(alarm, door, true, 1000));
        CHECK_FALSE(doorAlarmAt(alarm, door, true, 300999));
        CHECK_TRUE(doorAlarmAt(alarm, door, true, 301000));
        CHECK_NEAR(alarm.readCommand(), 1.0, 0.0);

        // Porte fermée : fin de l'alarme (non mémorisée).
        CHECK_FALSE(doorAlarmAt(alarm, door, false, 302000));

        // Réouverture : la tempo repart de zéro.
        CHECK_FALSE(doorAlarmAt(alarm, door, true, 303000));
        CHECK_FALSE(doorAlarmAt(alarm, door, true, 602999));
        CHECK_TRUE(doorAlarmAt(alarm, door, true, 603000));

        // Désactivée : plus d'alarme.
        alarm.settings.enabled = false;
        CHECK_FALSE(doorAlarmAt(alarm, door, true, 604000));
    }

    void testDoorInputFault()
    {
        DigitalInput door;
        door.begin("porte", "Porte", PIN_1);

        ConditionAlarm alarm;
        alarm.begin("alarme_porte", "Porte ouverte", door);
        alarm.settings.enabled = true;

        // Entrée pas encore lue : ni alarme ni défaut.
        alarm.update(0);
        CHECK_FALSE(alarm.isActive());

        CHECK_FALSE(doorAlarmAt(alarm, door, false, 1000));

        // Entrée invalide après avoir été lue (polarité changée au menu,
        // pas encore appliquée) : défaut signalé.
        door.settings.activeHigh = false;
        CHECK_FALSE(door.isValid());
        alarm.update(2000);
        CHECK_TRUE(alarm.isActive());

        // Après une reprise (menu), l'entrée doit être relue.
        alarm.resume(3000);
        alarm.update(3000);
        CHECK_FALSE(alarm.isActive());

        // Relue (porte fermée, polarité inversée : niveau HIGH).
        FakeDigitalIO::levels[PIN_1] = HIGH;
        door.poll(4000);
        door.poll(4001);
        alarm.update(4001);
        CHECK_TRUE(door.isValid());
        CHECK_FALSE(alarm.isActive());
    }

    // Glue qui écrit (ou oublie) la condition d'une alarme.
    class AlarmLogic final : public ProcessLogic
    {
    public:
        ConditionAlarm* alarm = nullptr;
        bool write = true;
        bool condition = false;

        void processLogic(uint32_t now) override
        {
            (void)now;

            if (write)
                alarm->set(condition);
        }

        void resumeLogic(uint32_t now) override
        {
            (void)now;
        }
    };

    struct GlueBench
    {
        ProcessControl process;
        ConditionAlarm alarm;
        AlarmLogic logic;

        GlueBench()
        {
            alarm.begin("defaut_cycle", "Defaut cycle");
            alarm.settings.enabled = true;
            logic.alarm = &alarm;
            process.add(alarm);
            process.setLogic(logic);
        }

        bool activeAt(uint32_t now)
        {
            process.updateMeasurementsAndRegulators(now);
            return alarm.isActive();
        }

        size_t events() const
        {
            return process.eventLog().count();
        }
    };

    void testGlueCondition()
    {
        GlueBench bench;

        CHECK_FALSE(bench.activeAt(1000));

        // Condition écrite par la glue : évaluée dès ce cycle.
        bench.logic.condition = true;
        CHECK_TRUE(bench.activeAt(2000));

        // Journal : « Alarme Defaut cycle : ACTIVE ».
        const EventEntry* entry = bench.process.eventLog().at(0);
        CHECK_TRUE(entry != nullptr);
        CHECK_TRUE(
            entry != nullptr &&
            std::strcmp(entry->text, "Alarme Defaut cycle : ACTIVE") == 0);

        bench.logic.condition = false;
        CHECK_FALSE(bench.activeAt(3000));
    }

    void testGlueMissingWrite()
    {
        GlueBench bench;
        CHECK_FALSE(bench.activeAt(1000));

        const size_t events = bench.events();

        // Condition non écrite : défaut, signalé et journalisé une fois.
        bench.logic.write = false;
        CHECK_TRUE(bench.activeAt(2000));

        bool logged = false;

        for (size_t i = 0; i < bench.events() - events; i++)
        {
            const EventEntry* entry = bench.process.eventLog().at(i);

            if (std::strcmp(entry->text, "Defaut cycle : non écrite") == 0)
                logged = true;
        }

        CHECK_TRUE(logged);

        const size_t afterMissing = bench.events();
        CHECK_TRUE(bench.activeAt(3000));
        CHECK_TRUE(bench.events() == afterMissing);

        // Inhibition : la condition est masquée, l'oubli ne l'est pas.
        bench.alarm.allowInhibit();
        bench.alarm.inhibit(true);
        CHECK_TRUE(bench.activeAt(4000));

        bench.logic.write = true;
        bench.logic.condition = true;
        CHECK_FALSE(bench.activeAt(5000));
    }

    void testLatchingAndAcknowledge()
    {
        GlueBench bench;
        bench.alarm.settings.latching = true;

        bench.logic.condition = true;
        CHECK_TRUE(bench.activeAt(1000));

        // Cause disparue : mémorisée jusqu'à l'acquittement.
        bench.logic.condition = false;
        CHECK_TRUE(bench.activeAt(2000));
        CHECK_TRUE(bench.alarm.isLatched());

        bench.alarm.acknowledge();
        CHECK_FALSE(bench.activeAt(3000));
    }

    void testLongDelay()
    {
        // Une fois confirmée, l'alarme le reste au-delà de 49 jours.
        GlueBench bench;
        bench.alarm.settings.delay = 10;
        bench.logic.condition = true;

        CHECK_FALSE(bench.activeAt(1000));
        CHECK_TRUE(bench.activeAt(11000));
        CHECK_TRUE(bench.activeAt(3000000000UL));
        CHECK_TRUE(bench.activeAt(5000));
    }

    void testParameters()
    {
        ConditionAlarm alarm;
        alarm.begin("alarme", "Alarme");

        Parameter storage[4];
        ParameterList list;
        list.begin(storage, 4);
        alarm.registerParameters(list);

        CHECK_FALSE(list.hasError());
        CHECK_TRUE(list.find("alarme", "enabled") != nullptr);
        CHECK_TRUE(list.find("alarme", "delay") != nullptr);
        CHECK_TRUE(list.find("alarme", "latching") != nullptr);

        const Parameter* enabled = list.find("alarme", "enabled");
        CHECK_TRUE(
            enabled != nullptr &&
            std::strcmp(enabled->categoryKey, "alarms") == 0);
    }
}

void runConditionAlarmTests()
{
    TestHarness::run("alarme sur condition : entrée TOR", testDoorInput);
    TestHarness::run("alarme sur condition : défaut d'entrée", testDoorInputFault);
    TestHarness::run("alarme sur condition : glue", testGlueCondition);
    TestHarness::run("alarme sur condition : non écrite", testGlueMissingWrite);
    TestHarness::run("alarme sur condition : mémorisation", testLatchingAndAcknowledge);
    TestHarness::run("alarme sur condition : tempo longue", testLongDelay);
    TestHarness::run("alarme sur condition : paramètres", testParameters);
}
