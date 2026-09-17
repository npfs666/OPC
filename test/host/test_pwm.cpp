#include "TestHarness.h"

#include <Hardware/pinout.h>
#include <Outputs/ActuatorPWM.h>
#include <Outputs/PWMOutput.h>
#include <ProcessControl.h>
#include <Regulator/Regulator.h>
#include <hardware/pwm.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <limits>

namespace
{
    constexpr uint8_t PIN_1 = Board::Rp2040::OUTPUT_3;
    constexpr uint8_t PIN_2 = Board::Rp2040::OUTPUT_4;

    double physicalDuty(uint8_t pin)
    {
        if (FakeGPIO::functions[pin] == GPIO_FUNC_SIO)
        {
            CHECK_TRUE(FakeGPIO::outputs[pin]);
            return FakeGPIO::levels[pin] ? 1.0 : 0.0;
        }

        const auto& slice = FakePWM::slices[pwm_gpio_to_slice_num(pin)];
        CHECK_TRUE(slice.enabled);
        return static_cast<double>(slice.levels[pin % 2]) / (slice.wrap + 1);
    }

    ParameterDraft& draft(
        ParameterEditor& editor, const char* owner, const char* key)
    {
        const ParameterDraft* found = editor.find(owner, key);
        CHECK_TRUE(found != nullptr);
        return const_cast<ParameterDraft&>(*found);
    }

    void testDutyAndPolarity()
    {
        PWMOutput output;
        PWMOutput::sharedSettings.frequency = 1000;
        output.begin("pwm_1", PIN_1);
        CHECK_TRUE(output.begin());
        CHECK_NEAR(physicalDuty(PIN_1), 0.0, 0.0);

        for (double command : {0.0, 0.25, 0.7, 1.0, -1.0, 2.0})
        {
            output.setCommand(command, 100);
            output.poll(100);
            CHECK_NEAR(physicalDuty(PIN_1), constrain(command, 0.0, 1.0), 0.001);
        }

        output.setCommand(std::numeric_limits<double>::quiet_NaN(), 200);
        output.poll(200);
        CHECK_NEAR(physicalDuty(PIN_1), 0.0, 0.0);

        output.settings.activeHigh = false;
        CHECK_TRUE(output.applySettings());
        CHECK_NEAR(physicalDuty(PIN_1), 1.0, 0.0);
        output.setCommand(0.25, 300);
        output.poll(300);
        CHECK_NEAR(physicalDuty(PIN_1), 0.75, 0.001);
        output.forceSafe();
        CHECK_NEAR(output.requestedCommand(), 0.0, 0.0);
        CHECK_NEAR(physicalDuty(PIN_1), 1.0, 0.0);

        output.settings.safeCommand = 0.2;
        CHECK_TRUE(output.applySettings());
        CHECK_NEAR(physicalDuty(PIN_1), 0.8, 0.001);

        output.settings.pin = Board::Rp2040::OUTPUT_1;
        CHECK_FALSE(output.applySettings());
        CHECK_FALSE(output.isHealthy());
        CHECK_NEAR(physicalDuty(PIN_1), 0.8, 0.001);
    }

    void testSharedFrequencyAndMenu()
    {
        PWMOutput first, second;
        PWMOutput::sharedSettings.frequency = 1000;
        first.begin("pwm_1", PIN_1);
        second.begin("pwm_2", PIN_2);
        CHECK_TRUE(first.begin());
        first.setCommand(0.25, 0);
        first.poll(0);
        CHECK_TRUE(second.begin());
        CHECK_NEAR(physicalDuty(PIN_1), 0.25, 0.001);
        second.setCommand(0.75, 0);
        second.poll(0);

        Parameter storage[10];
        ParameterList parameters;
        parameters.begin(storage, 10);
        first.registerParameters(parameters);
        second.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());
        CHECK_TRUE(parameters.count() == 7);
        const Parameter* pin = parameters.find("pwm_1", "pin");
        CHECK_TRUE(pin->data.selection.count == 2);
        CHECK_TRUE(pin->data.selection.options[0].value == PIN_1);
        CHECK_TRUE(pin->data.selection.options[1].value == PIN_2);

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();
        CHECK_TRUE(first.validateParameters(editor));
        CHECK_TRUE(second.validateParameters(editor));
        draft(editor, "pwm_1", "pin").selectionValue = PIN_2;
        CHECK_FALSE(first.validateParameters(editor));
        CHECK_FALSE(second.validateParameters(editor));
        draft(editor, "pwm_1", "pin").selectionValue = PIN_1;

        draft(editor, "pwm", "frequency").integerValue = 25000;
        draft(editor, "pwm_1", "safe_command").numberValue = 0.4;
        CHECK_TRUE(editor.validate());
        CHECK_TRUE(editor.apply());
        CHECK_TRUE(first.applySettings());
        CHECK_NEAR(physicalDuty(PIN_1), 0.4, 0.001);
        CHECK_NEAR(physicalDuty(PIN_2), 0.75, 0.001);
        const auto& slice = FakePWM::slices[pwm_gpio_to_slice_num(PIN_1)];
        CHECK_NEAR(150000000.0 / (slice.divider * (slice.wrap + 1)), 25000, 1);

        draft(editor, "pwm", "frequency").integerValue = 0;
        CHECK_FALSE(editor.validate());
        PWMOutput::sharedSettings.frequency = 0;
        CHECK_FALSE(first.applySettings());
        PWMOutput::sharedSettings.frequency = 1000;
    }

    class TestRegulator : public Regulator
    {
    public:
        double_t value = 0.0;
        bool valid = true;

        void update(uint32_t) override
        {
            if (valid)
                writeCommand(value);
            else
                invalidateCommand();
        }
    };

    void testActuatorAndSafeState()
    {
        TestRegulator regulator;
        regulator.begin("regulator");
        regulator.value = 0.37;
        ActuatorPWM actuator;
        actuator.begin("actuator", regulator);
        PWMOutput first, second;
        first.begin("pwm_1", PIN_1);
        second.begin("pwm_2", PIN_2);
        ProcessControl process;
        CHECK_TRUE(process.add(regulator));
        CHECK_TRUE(process.add(actuator));
        CHECK_TRUE(process.connect(actuator, first));
        CHECK_TRUE(process.connect(actuator, second));
        CHECK_TRUE(process.beginOutputs());
        process.updateMeasurementsAndRegulators(100);
        CHECK_NEAR(physicalDuty(PIN_1), 0.37, 0.001);
        CHECK_NEAR(physicalDuty(PIN_2), 0.37, 0.001);

        regulator.valid = false;
        process.updateMeasurementsAndRegulators(200);
        CHECK_NEAR(physicalDuty(PIN_1), 0.0, 0.0);
        CHECK_NEAR(physicalDuty(PIN_2), 0.0, 0.0);

        regulator.valid = true;
        process.updateMeasurementsAndRegulators(300);
        process.forceSafeOutputs();
        CHECK_NEAR(physicalDuty(PIN_1), 0.0, 0.0);
        CHECK_NEAR(physicalDuty(PIN_2), 0.0, 0.0);
    }
}

void runPWMTests()
{
    TestHarness::run("PWM rapport cyclique, polarité et repli", testDutyAndPolarity);
    TestHarness::run("PWM fréquence partagée et menu", testSharedFrequencyAndMenu);
    TestHarness::run("PWM actionneur et sécurité régulation", testActuatorAndSafeState);
}
