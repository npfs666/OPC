#include "TestHarness.h"

#include <Hardware/pinout.h>
#include <Outputs/RelayOutput.h>
#include <Outputs/TimeProportionalActuator.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cstring>

namespace
{
    constexpr uint8_t PIN_1 = Board::Rp2040::OUTPUT_1;
    constexpr uint8_t PIN_2 = Board::Rp2040::OUTPUT_2;

    // Régulateur dont la commande est fixée par le test.
    class FixedRegulator final : public Regulator
    {
    public:
        void update(uint32_t) override
        {
        }

        void set(double_t command)
        {
            writeCommand(command);
        }
    };

    void setTime(uint32_t milliseconds)
    {
        FakeTime::milliseconds = milliseconds;
    }

    bool relayIsOn(const RelayOutput& relay)
    {
        return relay.appliedCommand() >= 0.5;
    }

    // Demande un état et laisse la sortie l'appliquer si elle le peut.
    void request(RelayOutput& relay, double_t command)
    {
        relay.setCommand(command, millis());
        relay.poll(millis());
    }

    void beginRelay(
        RelayOutput& relay,
        uint32_t minOnTime,
        uint32_t minOffTime,
        bool safeState = false)
    {
        relay.begin("relay", "Relais", PIN_1, true, safeState);
        relay.settings.minOnTime = minOnTime;
        relay.settings.minOffTime = minOffTime;
        relay.begin();
    }

    void testWithoutMinimumTimes()
    {
        setTime(5000);
        RelayOutput relay;
        beginRelay(relay, 0, 0);

        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));
        CHECK_TRUE(FakeDigitalIO::levels[PIN_1] == HIGH);
        CHECK_FALSE(relay.isWaiting());

        request(relay, 0.0);
        CHECK_FALSE(relayIsOn(relay));
        CHECK_TRUE(FakeDigitalIO::levels[PIN_1] == LOW);
    }

    void testMinimumTimes()
    {
        // Mise en service à t = 1 s : l'arrêt mini protège le démarrage.
        setTime(1000);
        RelayOutput relay;
        beginRelay(relay, 10, 20);

        request(relay, 1.0);
        CHECK_FALSE(relayIsOn(relay));
        CHECK_TRUE(relay.isWaiting());

        setTime(20999);
        request(relay, 1.0);
        CHECK_FALSE(relayIsOn(relay));

        setTime(21000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));
        CHECK_FALSE(relay.isWaiting());

        // Marche mini : 10 s depuis 21 s.
        setTime(25000);
        request(relay, 0.0);
        CHECK_TRUE(relayIsOn(relay));
        CHECK_TRUE(relay.isWaiting());

        setTime(31000);
        request(relay, 0.0);
        CHECK_FALSE(relayIsOn(relay));

        // Arrêt mini : 20 s depuis 31 s.
        setTime(40000);
        request(relay, 1.0);
        CHECK_FALSE(relayIsOn(relay));

        setTime(51000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));

        // Une demande annulée avant l'échéance ne laisse rien en attente.
        setTime(52000);
        request(relay, 0.0);
        CHECK_TRUE(relay.isWaiting());
        request(relay, 1.0);
        CHECK_FALSE(relay.isWaiting());
        CHECK_TRUE(relayIsOn(relay));
    }

    void testSafeStateBypassesMinimumOnTime()
    {
        setTime(0);
        RelayOutput relay;
        beginRelay(relay, 60, 30);

        setTime(30000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));

        // La sécurité coupe sans attendre la marche mini...
        setTime(31000);
        relay.forceSafe();
        CHECK_FALSE(relayIsOn(relay));
        CHECK_TRUE(FakeDigitalIO::levels[PIN_1] == LOW);
        CHECK_FALSE(relay.isWaiting());

        // ... et l'arrêt mini protège le redémarrage.
        setTime(60999);
        request(relay, 1.0);
        CHECK_FALSE(relayIsOn(relay));

        setTime(61000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));

        // État sûr à ON : appliqué même pendant un arrêt mini.
        setTime(0);
        RelayOutput pump;
        beginRelay(pump, 0, 300, true);
        CHECK_TRUE(relayIsOn(pump));
    }

    void testSettingsApplyKeepsTimer()
    {
        setTime(0);
        RelayOutput relay;
        beginRelay(relay, 0, 20);

        setTime(20000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));

        // Validation du menu : état sûr, puis l'arrêt mini court depuis 25 s.
        setTime(25000);
        CHECK_TRUE(relay.applySettings());
        CHECK_FALSE(relayIsOn(relay));

        // Une nouvelle validation ne relance pas le chrono.
        setTime(30000);
        CHECK_TRUE(relay.applySettings());

        setTime(45000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));

        // Une nouvelle broche repart comme au démarrage.
        setTime(50000);
        relay.settings.pin = PIN_2;
        CHECK_TRUE(relay.applySettings());

        setTime(69999);
        request(relay, 1.0);
        CHECK_FALSE(relayIsOn(relay));

        setTime(70000);
        request(relay, 1.0);
        CHECK_TRUE(relayIsOn(relay));
        CHECK_TRUE(FakeDigitalIO::levels[PIN_2] == HIGH);
    }

    void testRelayParameters()
    {
        RelayOutput relay;
        relay.begin("relay", "Relais", PIN_1, true, false);

        Parameter storage[8];
        ParameterList parameters;
        parameters.begin(storage, 8);
        relay.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());

        const Parameter* minOn = parameters.find("relay", "min_on_time");
        const Parameter* minOff = parameters.find("relay", "min_off_time");
        CHECK_TRUE(minOn != nullptr);
        CHECK_TRUE(minOff != nullptr);
    }

    void testWaitingSnapshot()
    {
        setTime(0);
        FixedRegulator regulator;
        regulator.begin("regulator");

        TimeProportionalActuator actuator;
        actuator.begin("actuator", "Actionneur", regulator, 10000);

        RelayOutput relay;
        relay.begin("relay", "Relais", PIN_1, true, false);
        relay.settings.minOffTime = 60;

        ProcessControl process;
        CHECK_TRUE(process.add(actuator));
        CHECK_TRUE(process.connect(actuator, relay));
        CHECK_TRUE(process.beginOutputs());

        regulator.set(1.0);
        setTime(1000);
        process.poll(1000);

        ProcessSnapshot snapshot;
        process.captureSnapshot(snapshot, 1000);
        const OutputSample* sample = snapshot.find(relay);
        CHECK_TRUE(sample != nullptr);
        if (sample != nullptr)
        {
            CHECK_TRUE(sample->waiting);
            CHECK_TRUE(sample->waitingToStart());
        }

        setTime(60000);
        process.poll(60000);
        process.captureSnapshot(snapshot, 60000);
        sample = snapshot.find(relay);
        CHECK_TRUE(sample != nullptr);
        if (sample != nullptr)
        {
            CHECK_FALSE(sample->waiting);
            CHECK_FALSE(sample->waitingToStart());
            CHECK_TRUE(sample->appliedCommand >= 0.5);
        }
    }

    // Durée à ON sur une période de 10 s, en tours de 100 ms.
    uint32_t onTimeOverPeriod(double_t command, uint32_t minPulse)
    {
        setTime(0);
        FixedRegulator regulator;
        regulator.begin("regulator");

        TimeProportionalActuator actuator;
        actuator.begin("actuator", "Actionneur", regulator, 10000, minPulse);

        RelayOutput relay;
        relay.begin("relay", "Relais", PIN_1, true, false);

        ProcessControl process;
        process.add(actuator);
        process.connect(actuator, relay);
        process.beginOutputs();

        regulator.set(command);

        uint32_t onTime = 0;

        for (uint32_t now = 0; now < 10000; now += 100)
        {
            setTime(now);
            process.poll(now);

            if (relayIsOn(relay))
                onTime += 100;
        }

        return onTime;
    }

    void testMinimumPulse()
    {
        // Sans impulsion mini : durée proportionnelle.
        CHECK_TRUE(onTimeOverPeriod(0.03, 0) == 300);
        CHECK_TRUE(onTimeOverPeriod(0.97, 0) == 9700);

        // Impulsion mini 500 ms sur 10 s.
        CHECK_TRUE(onTimeOverPeriod(0.0, 500) == 0);
        CHECK_TRUE(onTimeOverPeriod(0.03, 500) == 0);
        CHECK_TRUE(onTimeOverPeriod(0.05, 500) == 500);
        CHECK_TRUE(onTimeOverPeriod(0.5, 500) == 5000);
        CHECK_TRUE(onTimeOverPeriod(0.95, 500) == 9500);
        CHECK_TRUE(onTimeOverPeriod(0.97, 500) == 10000);
        CHECK_TRUE(onTimeOverPeriod(1.0, 500) == 10000);
    }

    void testMinimumPulseKeepsImmediateSwitching()
    {
        // Autotune : commande 0/1 en milieu de période, basculement immédiat.
        setTime(0);
        FixedRegulator regulator;
        regulator.begin("regulator");

        TimeProportionalActuator actuator;
        actuator.begin("actuator", "Actionneur", regulator, 10000, 500);

        RelayOutput relay;
        relay.begin("relay", "Relais", PIN_1, true, false);

        ProcessControl process;
        process.add(actuator);
        process.connect(actuator, relay);
        process.beginOutputs();

        regulator.set(0.0);
        process.poll(0);
        CHECK_FALSE(relayIsOn(relay));

        regulator.set(1.0);
        setTime(4200);
        process.poll(4200);
        CHECK_TRUE(relayIsOn(relay));

        regulator.set(0.0);
        setTime(4300);
        process.poll(4300);
        CHECK_FALSE(relayIsOn(relay));
    }

    void testMinimumPulseValidation()
    {
        FixedRegulator regulator;
        regulator.begin("regulator");

        // begin() limite l'impulsion à la moitié de la période.
        TimeProportionalActuator actuator;
        actuator.begin("actuator", "Actionneur", regulator, 2000, 1500);
        CHECK_TRUE(actuator.settings.minPulse == 1000);

        actuator.begin("actuator", "Actionneur", regulator, 10000, 500);

        Parameter storage[4];
        ParameterList parameters;
        parameters.begin(storage, 4);
        actuator.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();

        ParameterDraft* minPulse = nullptr;

        for (size_t i = 0; i < editor.count(); i++)
        {
            const Parameter* parameter = editor.get(i).parameter;

            if (parameter != nullptr &&
                std::strcmp(parameter->key, "min_pulse") == 0)
            {
                minPulse = &editor.get(i);
            }
        }

        CHECK_TRUE(minPulse != nullptr);
        if (minPulse == nullptr)
            return;

        CHECK_TRUE(actuator.validateParameters(editor));

        minPulse->integerValue = 5000;
        CHECK_TRUE(actuator.validateParameters(editor));

        minPulse->integerValue = 5100;
        CHECK_FALSE(actuator.validateParameters(editor));
    }
}

void runRelayTimingTests()
{
    TestHarness::run("relais : sans temps mini", testWithoutMinimumTimes);
    TestHarness::run("relais : marche et arret mini", testMinimumTimes);
    TestHarness::run("relais : etat sur prioritaire", testSafeStateBypassesMinimumOnTime);
    TestHarness::run("relais : validation menu et broche", testSettingsApplyKeepsTimer);
    TestHarness::run("relais : parametres", testRelayParameters);
    TestHarness::run("relais : attente dans le snapshot", testWaitingSnapshot);
    TestHarness::run("actionneur temporel : impulsion mini", testMinimumPulse);
    TestHarness::run("actionneur temporel : basculement immediat", testMinimumPulseKeepsImmediateSwitching);
    TestHarness::run("actionneur temporel : validation", testMinimumPulseValidation);
}
