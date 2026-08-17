#include "TestHarness.h"

#include <Adafruit_BME280.h>
#include <Installation.h>
#include <Measurements/Humidity/HumidityBME.h>
#include <ProcessSnapshot.h>
#include <Measurements/Pressure/PressureBME.h>
#include <Measurements/Temperature/Temperature.h>
#include <Measurements/Temperature/TemperatureBME.h>
#include <Outputs/Actuator.h>
#include <Outputs/Output.h>
#include <Outputs/RelayOutput.h>
#include <Physics/PT100.h>
#include <Physics/Psychrometrics.h>
#include <ProcessControl.h>
#include <Regulator/PID.h>
#include <Regulator/SolarRegulator.h>
#include <Regulator/Thermostat.h>
#include <SystemWatchdog.h>
#include <hmi/MenuBuilder.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cstring>
#include <limits>

namespace
{
    class MenuTestInstallation final :
        public Installation
    {
    public:
        const char* name() const override
        {
            return "Menu test";
        }

        const char* configurationKey() const override
        {
            return "test_installation";
        }

        bool begin(
            SensorBoard&,
            Adafruit_BME280&,
            ProcessControl&) override
        {
            return true;
        }

        void printHomeScreen(
            HomeScreenContext&) override
        {
        }

        bool registerTestParameters()
        {
            parameterList.begin(
                parameterStorage,
                MAX_PARAMETERS);

            auto input = parameterList.forOwner({
                "inputs",
                "Input",
                "input_1",
                "Input 1"
            });

            auto calibration = parameterList.forOwner({
                "calibration",
                "Calibration",
                "sensor_board",
                "Carte capteurs"
            });

            auto regulator = parameterList.forOwner({
                "regulators",
                "Regulateur",
                "pid",
                "PID"
            });

            auto autoTune = parameterList.forOwner({
                "regulators",
                "Regulateur",
                "pid.autotune",
                "PID autotune"
            });

            auto menu = parameterList.forOwner({
                "interface",
                "Interface",
                "menu",
                "Menu"
            });

            auto watchdog = parameterList.forOwner({
                "safety",
                "Sécurité",
                "measurement_watchdog",
                "Surveillance mesures"
            });

            return
                input.addInteger(
                    "samples",
                    "Samples",
                    samples,
                    1,
                    128,
                    1) &&
                calibration.addDouble(
                    "reference",
                    "Rref",
                    reference,
                    "Ω",
                    true) &&
                regulator.addDouble(
                    "setpoint",
                    "Consigne",
                    setpoint,
                    0.0,
                    100.0,
                    0.1,
                    1,
                    "°C") &&
                autoTune.addDouble(
                    "noise_band",
                    "Demi-bande",
                    noiseBand,
                    0.05,
                    10.0,
                    0.05,
                    2,
                    "°C") &&
                menu.addInteger(
                    "timeout",
                    "Timeout",
                    menuTimeout,
                    1,
                    120,
                    1,
                    "s") &&
                watchdog.addInteger(
                    "timeout",
                    "Timeout mesures",
                    measurementTimeout,
                    10,
                    300,
                    1,
                    "s");
        }

        bool addMenuActions(
            MenuBuilder& menu) const override
        {
            const MenuBuilder::GroupId group =
                menu.findGroupForOwner(
                    "pid.autotune");

            return
                group != MenuBuilder::INVALID_GROUP &&
                menu.addAction(
                    group,
                    1,
                    "pid_autotune_start",
                    "Lancer autotune") &&
                menu.addAction(
                    group,
                    2,
                    "pid_autotune_cancel",
                    "Annuler autotune");
        }

    private:
        uint16_t samples = 16;
        double_t reference = 1649.819;
        double_t setpoint = 20.0;
        double_t noiseBand = 0.5;
        uint32_t menuTimeout = 10;
        uint32_t measurementTimeout = 30;
    };

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
            Regulator& regulator)
        {
            Actuator::begin(
                "actuator",
                regulator);
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
        CHECK_TRUE(
            pid.settings.mode ==
                PID::Mode::Heating);
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

    void testCoolingPID()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        CHECK_TRUE(
            pid.setMode(PID::Mode::Cooling));

        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.1;
        pid.settings.ki = 0.0;
        pid.settings.kd = 0.0;

        measurement.setReading(15.0);
        pid.update(0);
        CHECK_FALSE(pid.isCommandValid());

        pid.update(1000);
        CHECK_NEAR(
            pid.readCommand(),
            0.5,
            0.0001);

        measurement.setReading(100.0);
        pid.update(2000);
        CHECK_NEAR(
            pid.readCommand(),
            1.0,
            0.0001);

        measurement.setReading(5.0);
        pid.update(3000);
        CHECK_NEAR(
            pid.readCommand(),
            0.0,
            0.0001);

        measurement.setReading(0.0, false);
        pid.update(4000);
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDModeChangeResetsController()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.1;

        measurement.setReading(5.0);
        pid.update(0);
        pid.update(1000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.5, 0.0001);

        CHECK_TRUE(
            pid.setMode(PID::Mode::Cooling));
        CHECK_FALSE(pid.isCommandValid());

        pid.update(2000);
        CHECK_FALSE(pid.isCommandValid());

        pid.update(3000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 0.0001);
    }

    void testCoolingPIDDerivative()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        CHECK_TRUE(
            pid.setMode(PID::Mode::Cooling));

        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.0;
        pid.settings.ki = 0.0;
        pid.settings.kd = 1.0;

        measurement.setReading(10.0);
        pid.update(0);

        measurement.setReading(11.0);
        pid.update(1000);
        CHECK_NEAR(
            pid.readCommand(),
            1.0,
            0.0001);

        measurement.setReading(10.5);
        pid.update(2000);
        CHECK_NEAR(
            pid.readCommand(),
            0.0,
            0.0001);
    }

    void testCoolingPIDAntiWindup()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        CHECK_TRUE(
            pid.setMode(PID::Mode::Cooling));

        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.0;
        pid.settings.ki = 0.1;
        pid.settings.kd = 0.0;

        measurement.setReading(15.0);
        pid.update(0);

        pid.update(1000);
        CHECK_NEAR(pid.readCommand(), 0.5, 0.0001);

        pid.update(2000);
        CHECK_NEAR(pid.readCommand(), 1.0, 0.0001);

        pid.update(3000);
        CHECK_NEAR(pid.readCommand(), 1.0, 0.0001);

        measurement.setReading(5.0);
        pid.update(4000);
        CHECK_NEAR(pid.readCommand(), 0.5, 0.0001);
    }

    void testPIDRejectsInvalidMode()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);

        CHECK_FALSE(
            pid.setMode(
                static_cast<PID::Mode>(99)));
        CHECK_TRUE(
            pid.settings.mode ==
                PID::Mode::Heating);

        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.1;

        measurement.setReading(5.0);
        pid.update(0);
        pid.update(1000);
        CHECK_TRUE(pid.isCommandValid());

        pid.settings.mode =
            static_cast<PID::Mode>(99);

        pid.update(2000);
        CHECK_FALSE(pid.isCommandValid());

        CHECK_FALSE(pid.startAutoTune(3000));
        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Failed);
        CHECK_TRUE(
            pid.getAutoTuneError() ==
                PID::AutoTuneError::InvalidSettings);
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDAutoTuneRejectsInvalidDirection()
    {
        PIDAutoTune autoTune;
        PIDAutoTune::Settings settings;

        CHECK_FALSE(
            autoTune.start(
                0,
                settings,
                10.0,
                0.0,
                1.0,
                static_cast<
                    PIDAutoTune::ProcessDirection>(99)));

        CHECK_TRUE(
            autoTune.getStatus() ==
                PIDAutoTune::Status::Failed);
        CHECK_TRUE(
            autoTune.getError() ==
                PIDAutoTune::Error::InvalidSettings);
        CHECK_FALSE(autoTune.hasCommand());
    }

    void testPIDAutotune()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 10.0;
        pid.autoTuneSettings.outputLow = 0.0;
        pid.autoTuneSettings.outputHigh = 1.0;
        pid.autoTuneSettings.noiseBand = 0.5;
        pid.autoTuneSettings.inputMin = 0.0;
        pid.autoTuneSettings.inputMax = 20.0;
        pid.autoTuneSettings.timeoutSeconds = 300;
        pid.autoTuneSettings.cycles = 2;

        pid.stop();
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());

        CHECK_TRUE(pid.startAutoTune(0));
        CHECK_TRUE(pid.isEnabled());
        CHECK_FALSE(pid.settings.enabled);
        CHECK_TRUE(pid.isAutoTuneActive());
        CHECK_FALSE(
            pid.setMode(PID::Mode::Cooling));
        CHECK_TRUE(
            pid.settings.mode ==
                PID::Mode::Heating);
        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::WaitingForMeasurement);

        measurement.setReading(10.0);
        pid.update(0);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Running);
        CHECK_NEAR(pid.readCommand(), 1.0, 0.0);

        const double_t readings[] = {
            9.0,
            10.6,
            11.0,
            9.4,
            9.0,
            10.6,
            11.0,
            9.4,
            9.0,
            10.6,
            11.0,
            9.4
        };

        for (size_t i = 0;
             i < sizeof(readings) /
                     sizeof(readings[0]);
             i++)
        {
            measurement.setReading(
                readings[i]);

            pid.update(
                static_cast<uint32_t>(
                    (i + 1) * 5000UL));
        }

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Succeeded);
        CHECK_FALSE(pid.isAutoTuneActive());
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
        CHECK_TRUE(
            pid.getAutoTuneCompletedCycles() == 2);

        const PID::AutoTuneResult& result =
            pid.getAutoTuneResult();

        CHECK_TRUE(result.ultimateGain > 0.0);
        CHECK_NEAR(
            result.ultimatePeriodSeconds,
            20.0,
            0.001);
        CHECK_NEAR(pid.settings.kp, result.kp, 0.0);
        CHECK_NEAR(pid.settings.ki, result.ki, 0.0);
        CHECK_NEAR(pid.settings.kd, result.kd, 0.0);
        CHECK_TRUE(
            pid.takeAutoTuneTuningsApplied());
        CHECK_FALSE(
            pid.takeAutoTuneTuningsApplied());

        pid.start();
        CHECK_TRUE(pid.isEnabled());
        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Idle);

        CHECK_FALSE(pid.setTunings(-1.0, 0.0, 0.0));
        CHECK_TRUE(pid.setTunings(1.0, 0.01, 2.0));
        CHECK_TRUE(pid.setOutputLimits(0.1, 0.9));
        CHECK_FALSE(pid.setOutputLimits(0.9, 0.1));
    }

    void testCoolingPIDAutotune()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        CHECK_TRUE(
            pid.setMode(PID::Mode::Cooling));

        pid.settings.setpoint = 10.0;
        pid.autoTuneSettings.outputLow = 0.2;
        pid.autoTuneSettings.outputHigh = 0.8;
        pid.autoTuneSettings.noiseBand = 0.5;
        pid.autoTuneSettings.inputMin = 0.0;
        pid.autoTuneSettings.inputMax = 20.0;
        pid.autoTuneSettings.timeoutSeconds = 300;
        pid.autoTuneSettings.cycles = 2;

        CHECK_TRUE(pid.startAutoTune(0));

        measurement.setReading(10.0);
        pid.update(0);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Running);
        CHECK_NEAR(pid.readCommand(), 0.8, 0.0);

        const double_t readings[] = {
            11.0,
            9.4,
            9.0,
            10.6,
            11.0,
            9.4,
            9.0,
            10.6,
            11.0,
            9.4,
            9.0,
            10.6
        };

        for (size_t i = 0;
             i < sizeof(readings) /
                     sizeof(readings[0]);
             i++)
        {
            measurement.setReading(readings[i]);
            pid.update(
                static_cast<uint32_t>(
                    (i + 1) * 5000UL));

            if (i == 0)
                CHECK_NEAR(pid.readCommand(), 0.8, 0.0);
            else if (i == 1)
                CHECK_NEAR(pid.readCommand(), 0.2, 0.0);
            else if (i == 3)
                CHECK_NEAR(pid.readCommand(), 0.8, 0.0);
        }

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Succeeded);
        CHECK_FALSE(pid.isAutoTuneActive());
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
        CHECK_TRUE(
            pid.getAutoTuneCompletedCycles() == 2);

        const PID::AutoTuneResult& result =
            pid.getAutoTuneResult();

        CHECK_NEAR(
            result.ultimateGain,
            0.4410631163,
            0.000001);
        CHECK_NEAR(
            result.ultimatePeriodSeconds,
            20.0,
            0.001);
        CHECK_NEAR(
            result.kp,
            0.2646378698,
            0.000001);
        CHECK_NEAR(
            result.ki,
            0.0264637870,
            0.000001);
        CHECK_NEAR(
            result.kd,
            0.6615946745,
            0.000001);
        CHECK_NEAR(
            pid.settings.kp,
            result.kp,
            0.0);
        CHECK_NEAR(
            pid.settings.ki,
            result.ki,
            0.0);
        CHECK_NEAR(
            pid.settings.kd,
            result.kd,
            0.0);
    }

    void testCoolingPIDAutotuneUsesPhysicalLimits()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        CHECK_TRUE(
            pid.setMode(PID::Mode::Cooling));

        pid.settings.setpoint = 10.0;
        pid.autoTuneSettings.inputMin = 0.0;
        pid.autoTuneSettings.inputMax = 20.0;

        CHECK_TRUE(pid.startAutoTune(0));

        /* -1 deviendrait +1 après normalisation : tester avant est vital. */
        measurement.setReading(-1.0);
        pid.update(0);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Failed);
        CHECK_TRUE(
            pid.getAutoTuneError() ==
                PID::AutoTuneError::InputOutOfRange);
        CHECK_FALSE(
            pid.takeAutoTuneTuningsApplied());
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDAutotuneStopsOnInvalidMeasurement()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 20.0;

        CHECK_TRUE(pid.startAutoTune(0));

        measurement.setReading(20.0);
        pid.update(0);
        CHECK_TRUE(pid.isCommandValid());

        measurement.setReading(0.0, false);
        pid.update(1000);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Failed);
        CHECK_TRUE(
            pid.getAutoTuneError() ==
                PID::AutoTuneError::InvalidMeasurement);
        CHECK_FALSE(
            pid.takeAutoTuneTuningsApplied());
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDAutotuneTimeoutIncludesWaiting()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 20.0;
        pid.autoTuneSettings.timeoutSeconds = 4;
        pid.autoTuneSettings.minimumCycleSeconds = 1;
        pid.autoTuneSettings.cycles = 2;

        const uint32_t startedAt =
            UINT32_MAX - 2000UL;

        CHECK_TRUE(
            pid.startAutoTune(startedAt));

        /* La mesure reste invalide et millis() traverse son rollover. */
        pid.update(1999);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Failed);
        CHECK_TRUE(
            pid.getAutoTuneError() ==
                PID::AutoTuneError::Timeout);
        CHECK_FALSE(
            pid.takeAutoTuneTuningsApplied());
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDAutotuneRejectsUnstableCycles()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 10.0;
        pid.autoTuneSettings.noiseBand = 0.5;
        pid.autoTuneSettings.inputMin = 0.0;
        pid.autoTuneSettings.inputMax = 20.0;
        pid.autoTuneSettings.timeoutSeconds = 300;
        pid.autoTuneSettings.minimumCycleSeconds = 1;
        pid.autoTuneSettings.stabilityTolerance = 0.05;
        pid.autoTuneSettings.cycles = 2;

        CHECK_TRUE(pid.startAutoTune(0));

        measurement.setReading(10.0);
        pid.update(0);

        const double_t readings[] = {
            8.0,
            10.6,
            12.0,
            9.4,
            9.0,
            10.6,
            11.0,
            9.4,
            8.0,
            10.6,
            12.0,
            9.4
        };

        for (size_t i = 0;
             i < sizeof(readings) /
                     sizeof(readings[0]);
             i++)
        {
            measurement.setReading(readings[i]);
            pid.update(
                static_cast<uint32_t>(
                    (i + 1) * 5000UL));
        }

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Running);

        measurement.setReading(10.0);
        pid.update(300000);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Failed);
        CHECK_TRUE(
            pid.getAutoTuneError() ==
                PID::AutoTuneError::InsufficientOscillation);
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDAutotuneInterruptedByResume()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 20.0;

        CHECK_TRUE(pid.startAutoTune(0));

        measurement.setReading(20.0);
        pid.update(0);
        CHECK_TRUE(pid.isCommandValid());

        pid.resume(1000);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Cancelled);
        CHECK_TRUE(
            pid.getAutoTuneError() ==
                PID::AutoTuneError::Interrupted);
        CHECK_FALSE(pid.isEnabled());
        CHECK_FALSE(pid.isCommandValid());
    }

    void testPIDAutotuneWaitingSurvivesResume()
    {
        FakeTemperature measurement;
        PID pid;

        pid.begin("pid", measurement);
        pid.settings.setpoint = 20.0;

        CHECK_TRUE(pid.startAutoTune(0));
        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::WaitingForMeasurement);

        /* Le lancement depuis le menu précède la reprise d'acquisition. */
        pid.resume(1000);

        CHECK_TRUE(pid.isAutoTuneActive());
        CHECK_TRUE(pid.isEnabled());
        CHECK_FALSE(pid.settings.enabled);

        measurement.setReading(20.0);
        pid.update(1000);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Running);
        CHECK_TRUE(pid.isCommandValid());
    }

    void testPIDAutotuneParametersAreOptIn()
    {
        FakeTemperature measurement;
        PID pid;
        Parameter storage[20];
        ParameterList parameters;

        pid.begin("pid", measurement);
        parameters.begin(storage, 20);

        pid.registerParameters(parameters);
        CHECK_TRUE(parameters.count() == 8);

        const Parameter* enabledParameter =
            parameters.find("pid", "enabled");

        const Parameter* modeParameter =
            parameters.find("pid", "mode");

        CHECK_TRUE(enabledParameter != nullptr);
        CHECK_TRUE(
            enabledParameter->type ==
                Parameter::Type::Bool);
        CHECK_TRUE(modeParameter != nullptr);
        CHECK_TRUE(
            modeParameter->type ==
                Parameter::Type::Selection);
        CHECK_TRUE(
            modeParameter->data.selection.count == 2);
        CHECK_TRUE(
            modeParameter->data.selection
                .options[0].value ==
                static_cast<int32_t>(
                    PID::Mode::Heating));
        CHECK_TRUE(
            modeParameter->data.selection
                .options[1].value ==
                static_cast<int32_t>(
                    PID::Mode::Cooling));

        CHECK_TRUE(
            pid.registerAutoTuneParameters(
                parameters,
                "pid.autotune",
                "PID autotune"));
        CHECK_TRUE(parameters.count() == 17);

        CHECK_TRUE(
            parameters.find(
                "pid.autotune",
                "autotune_noise_band") !=
            nullptr);
        CHECK_TRUE(
            parameters.find(
                "pid",
                "autotune_noise_band") ==
            nullptr);
        CHECK_TRUE(
            parameters.find(
                "pid.autotune",
                "pid_autotune_start") ==
            nullptr);

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();

        CHECK_TRUE(editor.validate());
        CHECK_TRUE(pid.validateParameters(editor));

        ParameterDraft* enabledDraft = nullptr;
        ParameterDraft* modeDraft = nullptr;

        for (size_t i = 0;
             i < editor.count();
             i++)
        {
            if (editor.get(i).parameter ==
                enabledParameter)
            {
                enabledDraft = &editor.get(i);
            }
            else if (editor.get(i).parameter ==
                modeParameter)
            {
                modeDraft = &editor.get(i);
            }
        }

        CHECK_TRUE(enabledDraft != nullptr);
        CHECK_TRUE(modeDraft != nullptr);

        enabledDraft->booleanValue = false;

        modeDraft->selectionValue =
            static_cast<int32_t>(
                PID::Mode::Cooling);

        CHECK_TRUE(editor.validate());
        CHECK_TRUE(editor.apply());
        CHECK_TRUE(
            pid.settings.mode ==
                PID::Mode::Cooling);
        CHECK_FALSE(pid.settings.enabled);

        modeDraft->selectionValue = 99;
        CHECK_FALSE(editor.validate());
    }

    void testRelaySafeStateLock()
    {
        RelayOutput relay;

        relay.begin(
            "heater_relay",
            "Heater relay",
            RELAIS_1,
            true,
            false);

        relay.lockSafeState(false);

        Parameter storage[4];
        ParameterList parameters;
        parameters.begin(storage, 4);
        relay.registerParameters(parameters);

        const Parameter* safeState =
            parameters.find(
                "heater_relay",
                "safe_state");

        CHECK_TRUE(safeState != nullptr);
        CHECK_TRUE(safeState->readOnly);

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();

        ParameterDraft* safeStateDraft = nullptr;

        for (size_t i = 0;
             i < editor.count();
             i++)
        {
            if (editor.get(i).parameter ==
                safeState)
            {
                safeStateDraft =
                    &editor.get(i);
                break;
            }
        }

        CHECK_TRUE(safeStateDraft != nullptr);

        safeStateDraft->booleanValue = true;
        CHECK_FALSE(
            relay.validateParameters(editor));

        safeStateDraft->booleanValue = false;
        CHECK_TRUE(
            relay.validateParameters(editor));

        relay.settings.safeState = true;
        relay.forceSafe();
        CHECK_FALSE(relay.settings.safeState);
        CHECK_NEAR(relay.appliedCommand(), 0.0, 0.0);

        relay.settings.safeState = true;
        CHECK_TRUE(relay.applySettings());
        CHECK_FALSE(relay.settings.safeState);
        CHECK_NEAR(relay.appliedCommand(), 0.0, 0.0);
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

        // readOnly bloque le menu, pas le logiciel ni la restauration.
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
            99.0,
            0.0001);

        editor.capture();
        editor.get(1).integerValue = 9;
        CHECK_FALSE(editor.validate());
    }

    void testMenuStructure()
    {
        MenuTestInstallation installation;
        MenuBuilder menu;

        CHECK_TRUE(
            installation.registerTestParameters());
        CHECK_TRUE(
            installation.buildMenu(menu));

        const MenuBuilder::GroupId root =
            menu.root();

        const MenuBuilder::GroupId inputs =
            menu.findSubmenu(
                root,
                "inputs");

        const MenuBuilder::GroupId miscellaneous =
            menu.findSubmenu(
                root,
                "miscellaneous");

        CHECK_TRUE(
            inputs != MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            miscellaneous !=
                MenuBuilder::INVALID_GROUP);

        const MenuBuilder::GroupId inputOne =
            menu.findGroupForOwner(
                "input_1");

        CHECK_TRUE(
            inputOne != MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            menu.getGroup(inputOne)->parent ==
                inputs);
        CHECK_TRUE(
            menu.findGroupForOwner(
                "measurement_watchdog") ==
                inputs);

        const MenuBuilder::GroupId regulators =
            menu.findSubmenu(
                root,
                "regulators");

        const MenuBuilder::GroupId pidGroup =
            menu.findGroupForOwner("pid");

        const MenuBuilder::GroupId autoTuneGroup =
            menu.findGroupForOwner(
                "pid.autotune");

        CHECK_TRUE(
            regulators !=
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            pidGroup !=
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            autoTuneGroup !=
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(pidGroup != autoTuneGroup);
        CHECK_TRUE(
            menu.getGroup(pidGroup)->parent ==
                regulators);
        CHECK_TRUE(
            menu.getGroup(autoTuneGroup)->parent ==
                regulators);

        CHECK_TRUE(menu.actionCount() == 2);

        const MenuBuilder::Action* startAction =
            menu.findAction(
                "pid_autotune_start");

        const MenuBuilder::Action* cancelAction =
            menu.findAction(2);

        CHECK_TRUE(startAction != nullptr);
        CHECK_TRUE(startAction->id == 1);
        CHECK_TRUE(
            startAction->group == autoTuneGroup);
        CHECK_TRUE(cancelAction != nullptr);
        CHECK_TRUE(
            cancelAction->group == autoTuneGroup);
        CHECK_TRUE(
            installation.getParameters().find(
                "pid.autotune",
                "pid_autotune_start") ==
            nullptr);

        CHECK_FALSE(
            menu.addAction(
                autoTuneGroup,
                MenuBuilder::NO_ACTION,
                "invalid_action",
                "Invalide"));
        CHECK_FALSE(
            menu.addAction(
                autoTuneGroup,
                1,
                "duplicate_id",
                "Doublon ID"));
        CHECK_FALSE(
            menu.addAction(
                autoTuneGroup,
                3,
                "pid_autotune_start",
                "Doublon clé"));

        const MenuBuilder::GroupId calibration =
            menu.findSubmenu(
                miscellaneous,
                "calibration");

        const MenuBuilder::GroupId menuSettings =
            menu.findSubmenu(
                miscellaneous,
                "menu");

        CHECK_TRUE(
            calibration !=
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            menuSettings !=
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            menu.findGroupForOwner(
                "sensor_board") ==
                calibration);
        CHECK_TRUE(
            menu.findGroupForOwner(
                "menu") ==
                menuSettings);

        CHECK_TRUE(
            menu.findSubmenu(
                root,
                "calibration") ==
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            menu.findSubmenu(
                root,
                "interface") ==
                MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(
            menu.findSubmenu(
                root,
                "safety") ==
                MenuBuilder::INVALID_GROUP);
    }

    void testInstallationIdentity()
    {
        MenuTestInstallation installation;

        CHECK_TRUE(
            strcmp(
                installation.configurationKey(),
                "test_installation") == 0);
        CHECK_TRUE(
            strcmp(
                installation.name(),
                installation.configurationKey()) != 0);
    }

    void testOutputStateUpdatedBeforeSnapshot()
    {
        ProcessControl process;
        FakeInputMeasurement input;
        FakeRegulator regulator;
        FakeOutput output;
        FakeActuator actuator(regulator);

        CHECK_TRUE(process.add(input));
        CHECK_TRUE(process.add(regulator));
        CHECK_TRUE(process.add(actuator));
        CHECK_TRUE(process.connect(
            actuator,
            output));

        ProcessSnapshot initialSnapshot;
        process.captureSnapshot(
            initialSnapshot,
            500);

        const OutputSample* initialState =
            initialSnapshot.find(output);

        CHECK_TRUE(initialState != nullptr);
        CHECK_FALSE(initialState->healthy);

        CHECK_TRUE(process.beginOutputs());

        input.nextValue = 12.0;
        regulator.nextCommand = 1.0;

        process.updateMeasurementsAndRegulators(
            1000);

        ProcessSnapshot snapshot;
        process.captureSnapshot(
            snapshot,
            1000);

        const OutputSample* state =
            snapshot.find(output);

        const MeasurementSample* measurement =
            snapshot.find(input);

        CHECK_TRUE(snapshot.measurementCount() == 1);
        CHECK_TRUE(snapshot.outputCount() == 1);
        CHECK_TRUE(snapshot.capturedAt() == 1000);
        CHECK_TRUE(measurement != nullptr);
        CHECK_TRUE(measurement->valid);
        CHECK_NEAR(
            measurement->value,
            12.0,
            0.0);
        CHECK_TRUE(state != nullptr);
        CHECK_TRUE(state->healthy);
        CHECK_NEAR(
            state->appliedCommand,
            1.0,
            0.0);
        CHECK_NEAR(
            output.appliedCommand(),
            1.0,
            0.0);
    }

    void testOutputConnections()
    {
        ProcessControl process;
        FakeRegulator regulator;
        FakeActuator firstActuator(regulator);
        FakeActuator secondActuator(regulator);
        FakeActuator unregisteredActuator(
            regulator);
        FakeOutput output;
        FakeOutput secondOutput;

        CHECK_TRUE(process.add(firstActuator));
        CHECK_TRUE(process.add(secondActuator));

        CHECK_FALSE(process.connect(
            unregisteredActuator,
            output));

        CHECK_TRUE(process.connect(
            firstActuator,
            output));
        CHECK_FALSE(process.connect(
            firstActuator,
            output));
        CHECK_FALSE(process.connect(
            secondActuator,
            output));

        CHECK_TRUE(process.connect(
            secondActuator,
            secondOutput));
    }

    void testSystemWatchdogRequiresBothCores()
    {
        rp2040.resetFakeState();
        rp2040.resetReason =
            RP2040::WDT_RESET;

        Stream diagnosticOutput;
        SystemWatchdog watchdog;

        watchdog.checkInUiCore();
        watchdog.checkInControlCore();

        CHECK_FALSE(rp2040.watchdogStarted);
        CHECK_TRUE(rp2040.watchdogResetCount == 0);

        watchdog.begin();
        watchdog.printLastResetDiagnostic(
            diagnosticOutput);

        CHECK_TRUE(rp2040.watchdogStarted);
        CHECK_TRUE(
            diagnosticOutput.printedLineCount == 1);
        CHECK_TRUE(
            rp2040.watchdogTimeoutMs ==
                SystemWatchdog::TIMEOUT_MS);

        watchdog.checkInControlCore();

        CHECK_TRUE(rp2040.watchdogResetCount == 0);

        watchdog.checkInUiCore();
        watchdog.checkInControlCore();

        CHECK_TRUE(rp2040.watchdogResetCount == 1);

        watchdog.checkInControlCore();

        CHECK_TRUE(rp2040.watchdogResetCount == 1);

        watchdog.checkInUiCore();
        watchdog.checkInControlCore();

        CHECK_TRUE(rp2040.watchdogResetCount == 2);
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
        "PID refroidissement",
        testCoolingPID);

    TestHarness::run(
        "changement de mode PID",
        testPIDModeChangeResetsController);

    TestHarness::run(
        "dérivée PID refroidissement",
        testCoolingPIDDerivative);

    TestHarness::run(
        "anti-windup PID refroidissement",
        testCoolingPIDAntiWindup);

    TestHarness::run(
        "rejet mode PID invalide",
        testPIDRejectsInvalidMode);

    TestHarness::run(
        "rejet sens autotune invalide",
        testPIDAutoTuneRejectsInvalidDirection);

    TestHarness::run(
        "autotune PID",
        testPIDAutotune);

    TestHarness::run(
        "autotune PID refroidissement",
        testCoolingPIDAutotune);

    TestHarness::run(
        "limites autotune PID refroidissement",
        testCoolingPIDAutotuneUsesPhysicalLimits);

    TestHarness::run(
        "sécurité autotune PID",
        testPIDAutotuneStopsOnInvalidMeasurement);

    TestHarness::run(
        "timeout attente autotune PID",
        testPIDAutotuneTimeoutIncludesWaiting);

    TestHarness::run(
        "rejet oscillations PID instables",
        testPIDAutotuneRejectsUnstableCycles);

    TestHarness::run(
        "interruption autotune PID",
        testPIDAutotuneInterruptedByResume);

    TestHarness::run(
        "attente autotune PID après menu",
        testPIDAutotuneWaitingSurvivesResume);

    TestHarness::run(
        "paramètres autotune PID optionnels",
        testPIDAutotuneParametersAreOptIn);

    TestHarness::run(
        "verrouillage sécurité relais",
        testRelaySafeStateLock);

    TestHarness::run(
        "éditeur de paramètres",
        testParameterEditor);

    TestHarness::run(
        "structure du menu",
        testMenuStructure);

    TestHarness::run(
        "identité de l'installation",
        testInstallationIdentity);

    TestHarness::run(
        "état sortie avant snapshot",
        testOutputStateUpdatedBeforeSnapshot);

    TestHarness::run(
        "connexion des sorties",
        testOutputConnections);

    TestHarness::run(
        "watchdog des deux coeurs",
        testSystemWatchdogRequiresBothCores);

    return TestHarness::finish();
}
