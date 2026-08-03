#include "TestHarness.h"

#include <Adafruit_BME280.h>
#include <Measurements/Humidity/HumidityBME.h>
#include <Measurements/MeasurementSnapshot.h>
#include <Measurements/Pressure/PressureBME.h>
#include <Measurements/Temperature/Temperature.h>
#include <Measurements/Temperature/TemperatureBME.h>
#include <Outputs/Actuator.h>
#include <Outputs/Output.h>
#include <Physics/PT100.h>
#include <Physics/Psychrometrics.h>
#include <ProcessControl.h>
#include <Regulator/PID.h>
#include <Regulator/SolarRegulator.h>
#include <Regulator/Thermostat.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <limits>

namespace
{
    class FakeTemperature final :
        public Temperature
    {
    public:
        FakeTemperature()
        {
            Temperature::begin("temperature");
        }

        void setReading(
            double_t value,
            bool valid = true)
        {
            setValue(value);
            setValid(valid);
        }

        void update() override
        {
        }
    };

    class FakeInputMeasurement final :
        public Measurement
    {
    public:
        FakeInputMeasurement()
        {
            Measurement::begin(
                "input",
                "");
        }

        void update() override
        {
            setValue(nextValue);
            setValid(true);
        }

        double_t nextValue = 0.0;
    };

    class FakeRegulator final :
        public Regulator
    {
    public:
        FakeRegulator()
        {
            Regulator::begin(
                "regulator");
        }

        void update(uint32_t) override
        {
            writeCommand(nextCommand);
        }

        double_t nextCommand = 0.0;
    };

    class FakeOutput final :
        public Output
    {
    public:
        FakeOutput()
        {
            Output::begin("output");
        }

        bool begin() override
        {
            initialized = true;
            forceSafe();
            return true;
        }

        void poll(uint32_t) override
        {
            if (initialized)
                setAppliedCommand(
                    requestedCommand());
        }

        void forceSafe() override
        {
            requested = 0.0;
            setAppliedCommand(0.0);
        }

        bool applySettings() override
        {
            return begin();
        }

        bool isHealthy() const override
        {
            return initialized;
        }

    private:
        bool initialized = false;
    };

    class FakeActuator final :
        public Actuator
    {
    public:
        FakeActuator(
            Regulator& regulator,
            Output& output)
        {
            Actuator::begin(
                "actuator",
                regulator);

            addOutput(output);
        }

        void update(uint32_t now) override
        {
            if (regulator == nullptr ||
                !regulator->isCommandValid())
            {
                for (uint8_t i = 0;
                     i < outputCount;
                     i++)
                {
                    outputs[i]->forceSafe();
                }

                return;
            }

            for (uint8_t i = 0;
                 i < outputCount;
                 i++)
            {
                outputs[i]->setCommand(
                    regulator->readCommand(),
                    now);
            }
        }
    };

    class OutputStateMeasurement final :
        public Measurement
    {
    public:
        explicit OutputStateMeasurement(
            Output& output)
            : output(output)
        {
            Measurement::begin(
                "output_state",
                "");
        }

        void update() override
        {
            setValue(
                output.appliedCommand());

            setValid(
                output.isHealthy());
        }

        UpdatePhase updatePhase()
            const override
        {
            return
                UpdatePhase::AfterOutputs;
        }

    private:
        Output& output;
    };

    void testBMEMeasurementPropagation()
    {
        Adafruit_BME280 bme;

        TemperatureBME temperature;
        HumidityBME humidity;
        PressureBME pressure;

        temperature.begin(
            "temperature",
            bme);

        humidity.begin(
            "humidity",
            bme);

        pressure.begin(
            "pressure",
            bme);

        bme.temperature = 21.5;
        bme.humidity = 48.25;
        bme.pressure = 101325.0;

        temperature.update();
        humidity.update();
        pressure.update();

        CHECK_TRUE(temperature.isValid());
        CHECK_TRUE(humidity.isValid());
        CHECK_TRUE(pressure.isValid());

        CHECK_NEAR(
            temperature.getValue(),
            21.5,
            0.0001);

        CHECK_NEAR(
            humidity.getValue(),
            48.25,
            0.0001);

        CHECK_NEAR(
            pressure.getValue(),
            101325.0,
            0.0001);

        bme.temperature =
            std::numeric_limits<float>::
                quiet_NaN();

        bme.humidity = 100.01f;
        bme.pressure = 29999.0f;

        temperature.update();
        humidity.update();
        pressure.update();

        CHECK_FALSE(temperature.isValid());
        CHECK_FALSE(humidity.isValid());
        CHECK_FALSE(pressure.isValid());

        bme.temperature = -40.0f;
        bme.humidity = 0.0f;
        bme.pressure = 30000.0f;

        temperature.update();
        humidity.update();
        pressure.update();

        CHECK_TRUE(temperature.isValid());
        CHECK_TRUE(humidity.isValid());
        CHECK_TRUE(pressure.isValid());

        bme.temperature = 85.0f;
        bme.humidity = 100.0f;
        bme.pressure = 110000.0f;

        temperature.update();
        humidity.update();
        pressure.update();

        CHECK_TRUE(temperature.isValid());
        CHECK_TRUE(humidity.isValid());
        CHECK_TRUE(pressure.isValid());

        bme.temperature = 85.01f;
        bme.humidity = -0.01f;
        bme.pressure = 110001.0f;

        temperature.update();
        humidity.update();
        pressure.update();

        CHECK_FALSE(temperature.isValid());
        CHECK_FALSE(humidity.isValid());
        CHECK_FALSE(pressure.isValid());
    }

    void testPT100Interpolation()
    {
        CHECK_NEAR(
            PT100::getResistanceToTemperature(
                100.0),
            0.0,
            0.0001);

        CHECK_NEAR(
            PT100::getResistanceToTemperature(
                110.0),
            25.686,
            0.001);

        const double_t midpoint =
            PT100::getResistanceToTemperature(
                105.0);

        CHECK_TRUE(midpoint > 0.0);
        CHECK_TRUE(midpoint < 25.686);

        CHECK_TRUE(
            std::isnan(
                PT100::
                    getResistanceToTemperature(
                        9.99)));

        CHECK_TRUE(
            std::isnan(
                PT100::
                    getResistanceToTemperature(
                        200.01)));
    }

    void testPsychrometrics()
    {
        CHECK_NEAR(
            Physics::Psychrometrics::getRH(
                20.0,
                20.0,
                101325.0),
            100.0,
            0.01);

        const double_t humidity =
            Physics::Psychrometrics::getRH(
                25.0,
                20.0,
                101325.0);

        CHECK_TRUE(humidity > 0.0);
        CHECK_TRUE(humidity < 100.0);

        CHECK_NEAR(
            Physics::Psychrometrics::dewPoint(
                20.0,
                100.0),
            20.0,
            0.01);

        CHECK_TRUE(
            Physics::Psychrometrics::
                absoluteHumidity(
                    20.0,
                    50.0) > 0.0);
    }

    void testHeatingThermostat()
    {
        FakeTemperature temperature;
        Thermostat thermostat;

        thermostat.begin(
            "thermostat",
            temperature);

        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        temperature.setReading(18.0);
        thermostat.update(0);
        CHECK_TRUE(
            thermostat.isCommandValid());
        CHECK_NEAR(
            thermostat.readCommand(),
            1.0,
            0.0);

        temperature.setReading(20.0);
        thermostat.update(1000);
        CHECK_NEAR(
            thermostat.readCommand(),
            1.0,
            0.0);

        temperature.setReading(22.0);
        thermostat.update(2000);
        CHECK_NEAR(
            thermostat.readCommand(),
            0.0,
            0.0);

        temperature.setReading(20.0);
        thermostat.update(3000);
        CHECK_NEAR(
            thermostat.readCommand(),
            0.0,
            0.0);

        temperature.setReading(20.0, false);
        thermostat.update(4000);
        CHECK_FALSE(
            thermostat.isCommandValid());
    }

    void testCoolingThermostat()
    {
        FakeTemperature temperature;
        Thermostat thermostat;

        thermostat.begin(
            "thermostat",
            temperature);

        thermostat.settings.mode =
            Thermostat::Mode::Cooling;
        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        temperature.setReading(22.0);
        thermostat.update(0);
        CHECK_NEAR(
            thermostat.readCommand(),
            1.0,
            0.0);

        temperature.setReading(18.0);
        thermostat.update(1000);
        CHECK_NEAR(
            thermostat.readCommand(),
            0.0,
            0.0);
    }

    void testSolarRegulator()
    {
        FakeTemperature collector;
        FakeTemperature tankTop;
        FakeTemperature tankBottom;
        SolarRegulator solar;

        solar.begin(
            "solar",
            collector,
            tankTop,
            tankBottom);

        collector.setReading(50.0);
        tankTop.setReading(40.0);
        tankBottom.setReading(40.0);
        solar.update(0);
        CHECK_NEAR(
            solar.readCommand(),
            1.0,
            0.0);

        collector.setReading(45.0);
        solar.update(1000);
        CHECK_NEAR(
            solar.readCommand(),
            1.0,
            0.0);

        collector.setReading(44.0);
        solar.update(2000);
        CHECK_NEAR(
            solar.readCommand(),
            0.0,
            0.0);

        collector.setReading(60.0);
        tankTop.setReading(80.0);
        solar.update(3000);
        CHECK_NEAR(
            solar.readCommand(),
            0.0,
            0.0);

        collector.setReading(60.0, false);
        solar.update(4000);
        CHECK_FALSE(
            solar.isCommandValid());
    }

    void testPID()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.1;
        pid.settings.ki = 0.0;
        pid.settings.kd = 0.0;

        measurement.setReading(5.0);
        pid.update(0);
        CHECK_FALSE(pid.isCommandValid());

        pid.update(1000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(
            pid.readCommand(),
            0.5,
            0.0001);

        measurement.setReading(-100.0);
        pid.update(2000);
        CHECK_NEAR(
            pid.readCommand(),
            1.0,
            0.0001);

        measurement.setReading(0.0, false);
        pid.update(3000);
        CHECK_FALSE(pid.isCommandValid());
    }

    void testParameterEditor()
    {
        Parameter storage[8];
        ParameterList list;

        list.begin(storage, 8);

        bool enabled = false;
        int32_t timeout = 30;
        double_t setpoint = 20.0;
        double_t displayedValue = 42.0;

        auto parameters =
            list.forOwner({
                "test",
                "Test",
                "owner",
                "Owner"
            });

        CHECK_TRUE(
            parameters.addBool(
                "enabled",
                "Enabled",
                enabled));

        CHECK_TRUE(
            parameters.addInteger(
                "timeout",
                "Timeout",
                timeout,
                10,
                300,
                1,
                "s"));

        CHECK_TRUE(
            parameters.addDouble(
                "setpoint",
                "Setpoint",
                setpoint,
                0.0,
                100.0,
                0.1,
                1,
                "°C"));

        CHECK_TRUE(
            parameters.addDouble(
                "displayed_value",
                "Displayed value",
                displayedValue,
                nullptr,
                true));

        CHECK_TRUE(list.get(3)->readOnly);
        CHECK_TRUE(
            list.get(3)->data.number.decimals == 3);

        ParameterEditor editor;
        editor.begin(list);
        editor.capture();

        editor.get(0).booleanValue = true;
        editor.get(1).integerValue = 10;
        editor.get(2).numberValue = 25.5;
        editor.get(3).numberValue = 99.0;

        CHECK_TRUE(editor.validate());
        CHECK_TRUE(editor.apply());
        CHECK_TRUE(enabled);
        CHECK_TRUE(timeout == 10);
        CHECK_NEAR(
            setpoint,
            25.5,
            0.0001);
        CHECK_NEAR(
            displayedValue,
            42.0,
            0.0001);

        editor.capture();
        editor.get(1).integerValue = 9;
        CHECK_FALSE(editor.validate());
    }

    void testOutputStateUpdatedBeforeSnapshot()
    {
        ProcessControl process;
        FakeInputMeasurement input;
        FakeRegulator regulator;
        FakeOutput output;
        FakeActuator actuator(
            regulator,
            output);
        OutputStateMeasurement outputState(
            output);

        CHECK_TRUE(process.add(input));
        CHECK_TRUE(process.add(regulator));
        CHECK_TRUE(process.add(actuator));
        CHECK_TRUE(process.add(output));
        CHECK_TRUE(process.add(outputState));
        CHECK_TRUE(process.beginOutputs());

        input.nextValue = 12.0;
        regulator.nextCommand = 1.0;

        process.updateMeasurementsAndRegulators(
            1000);

        MeasurementSnapshot snapshot;
        process.captureMeasurements(
            snapshot,
            1000);

        const MeasurementSample* state =
            snapshot.find(outputState);

        CHECK_TRUE(state != nullptr);
        CHECK_TRUE(state->valid);
        CHECK_NEAR(
            state->value,
            1.0,
            0.0);
        CHECK_NEAR(
            output.appliedCommand(),
            1.0,
            0.0);
    }
}

int main()
{
    TestHarness::run(
        "validation des mesures BME280",
        testBMEMeasurementPropagation);

    TestHarness::run(
        "interpolation PT100",
        testPT100Interpolation);

    TestHarness::run(
        "psychrométrie",
        testPsychrometrics);

    TestHarness::run(
        "thermostat chauffage",
        testHeatingThermostat);

    TestHarness::run(
        "thermostat refroidissement",
        testCoolingThermostat);

    TestHarness::run(
        "régulateur solaire",
        testSolarRegulator);

    TestHarness::run(
        "PID",
        testPID);

    TestHarness::run(
        "éditeur de paramètres",
        testParameterEditor);

    TestHarness::run(
        "état sortie avant snapshot",
        testOutputStateUpdatedBeforeSnapshot);

    return TestHarness::finish();
}
