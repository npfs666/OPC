#include "TestHarness.h"

#include <Hardware/pinout.h>
#include <Inputs/DigitalInput.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr uint8_t PIN_1 = Board::Rp2040::DIGITAL_INPUT_1;
    constexpr uint8_t PIN_2 = Board::Rp2040::DIGITAL_INPUT_2;

    void testInitializationAndPolarity()
    {
        DigitalInput input;
        input.poll(0);
        CHECK_FALSE(input.isValid());
        CHECK_FALSE(input.isActive());
        CHECK_TRUE(std::isnan(input.printValue()));

        FakeDigitalIO::modes[PIN_1] = OUTPUT;
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.begin("input", PIN_1);
        CHECK_TRUE(FakeDigitalIO::modes[PIN_1] == INPUT);
        CHECK_FALSE(input.isValid());
        input.poll(10);
        CHECK_TRUE(input.isValid());
        CHECK_TRUE(input.isActive());
        CHECK_TRUE(input.sampledAt() == 10);

        FakeDigitalIO::levels[PIN_1] = LOW;
        input.poll(11);
        CHECK_FALSE(input.isActive());
        CHECK_TRUE(input.isValid());

        input.begin("input", PIN_1, false);
        CHECK_FALSE(input.isValid());
        input.poll(12);
        CHECK_TRUE(input.isActive());
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.poll(13);
        CHECK_FALSE(input.isActive());
    }

    void testDebounceAndWraparound()
    {
        DigitalInput input;
        input.begin("input", PIN_1, true, 20);
        FakeDigitalIO::levels[PIN_1] = LOW;
        input.poll(100);
        input.poll(119);
        CHECK_FALSE(input.isValid());
        input.poll(120);
        CHECK_TRUE(input.isValid());
        CHECK_FALSE(input.isActive());

        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.poll(130);
        FakeDigitalIO::levels[PIN_1] = LOW;
        input.poll(140);
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.poll(145);
        input.poll(164);
        CHECK_FALSE(input.isActive());
        input.poll(165);
        CHECK_TRUE(input.isActive());

        FakeDigitalIO::levels[PIN_1] = LOW;
        input.poll(170);
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.poll(180);
        FakeDigitalIO::levels[PIN_1] = LOW;
        input.poll(190);
        input.poll(209);
        CHECK_TRUE(input.isActive());
        input.poll(210);
        CHECK_FALSE(input.isActive());

        input.begin("input", PIN_1, true, 20);
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.poll(UINT32_MAX - 9);
        input.poll(9);
        CHECK_FALSE(input.isValid());
        input.poll(10);
        CHECK_TRUE(input.isValid());
        CHECK_TRUE(input.isActive());
    }

    void testSettings()
    {
        DigitalInput input;
        input.begin("permit", "Autorisation", PIN_1);
        FakeDigitalIO::levels[PIN_1] = HIGH;
        input.poll(0);
        CHECK_TRUE(input.isActive());

        ProcessControl process;
        CHECK_TRUE(process.add(input));
        Parameter storage[4];
        ParameterList parameters;
        parameters.begin(storage, 4);
        process.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());
        CHECK_TRUE(parameters.count() == 2);
        CHECK_TRUE(parameters.find("permit", "pin") == nullptr);

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();
        auto* polarity = const_cast<ParameterDraft*>(
            editor.find("permit", "active_high"));
        auto* debounce = const_cast<ParameterDraft*>(
            editor.find("permit", "debounce_ms"));
        CHECK_TRUE(polarity != nullptr && debounce != nullptr);
        if (polarity == nullptr || debounce == nullptr)
            return;

        polarity->booleanValue = false;
        debounce->integerValue = 20;
        CHECK_TRUE(editor.validate());
        CHECK_TRUE(editor.apply());
        CHECK_FALSE(input.isValid());
        CHECK_FALSE(input.isActive());
        input.poll(10);
        input.poll(29);
        CHECK_FALSE(input.isValid());
        input.poll(30);
        CHECK_TRUE(input.isValid());
        CHECK_FALSE(input.isActive());

        // Une consultation sans modification ne redémarre pas le filtre.
        editor.capture();
        CHECK_TRUE(editor.apply());
        input.poll(31);
        CHECK_TRUE(input.isValid());

        debounce->integerValue = 10001;
        CHECK_FALSE(editor.validate());
        debounce->integerValue = -1;
        CHECK_FALSE(editor.validate());
    }

    class InputRegulator : public Regulator
    {
    public:
        explicit InputRegulator(const DigitalInput& input) : input(input) {}

        void update(uint32_t) override
        {
            if (input.isValid())
                writeCommand(input.isActive() ? 1.0 : 0.0);
            else
                invalidateCommand();
        }

    private:
        const DigitalInput& input;
    };

    void testProcessAndSnapshots()
    {
        DigitalInput first, second, extra;
        first.begin("first", PIN_1);
        second.begin("second", PIN_2);
        ProcessControl process;
        CHECK_TRUE(process.add(first));
        CHECK_FALSE(process.add(first));
        CHECK_TRUE(process.add(second));
        CHECK_FALSE(process.add(extra));

        InputRegulator regulator(first);
        CHECK_TRUE(process.add(regulator));
        ProcessSnapshot initial;
        process.captureSnapshot(initial, 0);
        CHECK_TRUE(initial.inputCount() == 2);
        CHECK_FALSE(initial.find(first)->valid);
        CHECK_TRUE(initial.find(extra) == nullptr);
        CHECK_TRUE(initial.inputAt(2) == nullptr);

        FakeDigitalIO::levels[PIN_1] = HIGH;
        FakeDigitalIO::levels[PIN_2] = LOW;
        process.updateMeasurementsAndRegulators(100);
        CHECK_TRUE(regulator.isCommandValid());
        CHECK_NEAR(regulator.readCommand(), 1.0, 0.0);

        ProcessSnapshot snapshot;
        process.captureSnapshot(snapshot, 100);
        CHECK_TRUE(snapshot.find(first)->active);
        CHECK_FALSE(snapshot.find(second)->active);
        CHECK_TRUE(snapshot.find(second)->valid);

        // Aucun cycle ADC ou pilotage de sortie requis pour actualiser l'entrée.
        FakeDigitalIO::levels[PIN_1] = LOW;
        process.forceSafeOutputs();
        process.pollInputs(200);
        CHECK_TRUE(snapshot.find(first)->active);
        process.captureInputSnapshot(snapshot);
        CHECK_FALSE(snapshot.find(first)->active);
        CHECK_TRUE(snapshot.find(first)->sampledAt == 200);
        CHECK_TRUE(snapshot.capturedAt() == 100);
        CHECK_FALSE(initial.find(first)->valid);

        ProcessControl empty;
        empty.captureInputSnapshot(snapshot);
        CHECK_TRUE(snapshot.inputCount() == 0);
        CHECK_TRUE(snapshot.find(first) == nullptr);
    }
}

void runDigitalInputTests()
{
    TestHarness::run("entrées numériques : initialisation et polarité", testInitializationAndPolarity);
    TestHarness::run("entrées numériques : rebonds et débordement millis", testDebounceAndWraparound);
    TestHarness::run("entrées numériques : paramètres et revalidation", testSettings);
    TestHarness::run("entrées numériques : processus et snapshots", testProcessAndSnapshots);
}
