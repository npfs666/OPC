#include "TestHarness.h"

#include <Measurements/Temperature/Temperature.h>
#include <Regulator/PID.h>
#include <Regulator/Thermostat.h>
#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    using Status = MeasurementStatus;
    using Action = Regulator::FaultAction;

    // Température dont la valeur et l'état sont fixés par le test.
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

    // Thermostat de chauffage 20 °C ± 1 °C, en marche à 18 °C à t = 0.
    void startHeating(Thermostat& thermostat, ControlledTemperature& temperature)
    {
        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        temperature.set(18.0);
        thermostat.update(0);
    }

    void testSafeStateByDefault()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);
        CHECK_TRUE(thermostat.faultSettings.action == Action::SafeState);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        temperature.fail(Status::Open);
        thermostat.update(1000);
        CHECK_FALSE(thermostat.isCommandValid());
        CHECK_FALSE(thermostat.isInFallback());
    }

    void testHoldIsLimited()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);
        thermostat.faultSettings.action = Action::Hold;
        thermostat.faultSettings.holdTime = 30;

        // Défaut à t = 1 s : la marche est maintenue 30 s.
        temperature.fail(Status::Open);
        thermostat.update(1000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_TRUE(thermostat.isInFallback());
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        thermostat.update(30999);
        CHECK_TRUE(thermostat.isInFallback());

        thermostat.update(31000);
        CHECK_FALSE(thermostat.isCommandValid());
        CHECK_FALSE(thermostat.isInFallback());

        // Le maintien ne revient pas tant que le défaut dure.
        thermostat.update(40000);
        CHECK_FALSE(thermostat.isCommandValid());

        // Retour de la mesure : régulation normale, nouveau maintien possible.
        temperature.set(18.0);
        thermostat.update(41000);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);
        CHECK_FALSE(thermostat.isInFallback());

        temperature.fail(Status::Short);
        thermostat.update(42000);
        CHECK_TRUE(thermostat.isInFallback());
    }

    void testHoldNeedsValidCommand()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);
        thermostat.faultSettings.action = Action::Hold;

        // Pas encore de mesure : état sûr.
        temperature.fail(Status::NotReady);
        thermostat.update(1000);
        CHECK_FALSE(thermostat.isCommandValid());

        // Reprise (menu) pendant un défaut : rien à maintenir.
        temperature.set(18.0);
        thermostat.update(2000);
        thermostat.resume(3000);
        temperature.fail(Status::Open);
        thermostat.update(4000);
        CHECK_FALSE(thermostat.isCommandValid());
    }

    void testPIDHoldIsBumpless()
    {
        ControlledTemperature temperature;
        PID pid;
        pid.begin("pid", temperature);
        pid.settings.setpoint = 50.0;
        pid.settings.kp = 0.05;
        pid.settings.ti = 100.0;
        pid.settings.td = 0.0;
        pid.faultSettings.action = Action::Hold;
        pid.faultSettings.holdTime = 60;

        temperature.set(45.0);
        for (uint32_t now = 0; now <= 20000; now += 1000)
            pid.update(now);

        const double_t before = pid.readCommand();
        CHECK_TRUE(before > 0.0 && before < 1.0);

        temperature.fail(Status::Open);
        pid.update(21000);
        CHECK_TRUE(pid.isInFallback());
        CHECK_NEAR(pid.readCommand(), before, 1e-12);

        // Retour avant la fin du maintien : la commande maintenue couvre le
        // cycle de réinitialisation, sans passer par l'état sûr...
        temperature.set(45.0);
        pid.update(22000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), before, 1e-12);

        // ... puis le calcul reprend avec l'intégrale conservée.
        pid.update(23000);
        CHECK_FALSE(pid.isInFallback());
        CHECK_NEAR(pid.readCommand(), before, 0.01);
    }

    void testFaultParameters()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);

        Parameter storage[8];
        ParameterList parameters;
        parameters.begin(storage, 8);
        thermostat.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());
        CHECK_TRUE(parameters.find("thermostat", "fault_action") != nullptr);
        CHECK_TRUE(parameters.find("thermostat", "fault_hold_time") != nullptr);

        // Verrouillé : action imposée, pas de réglage dans le menu.
        Thermostat heater;
        heater.begin("heater", "Appoint", temperature);
        heater.faultSettings.action = Action::Hold;
        heater.lockFaultAction(Action::SafeState);
        CHECK_TRUE(heater.faultSettings.action == Action::SafeState);

        Parameter lockedStorage[8];
        ParameterList locked;
        locked.begin(lockedStorage, 8);
        heater.registerParameters(locked);
        CHECK_TRUE(locked.find("heater", "fault_action") == nullptr);
    }
}

void runFaultFallbackTests()
{
    TestHarness::run("repli : securite par defaut", testSafeStateByDefault);
    TestHarness::run("repli : maintien limite", testHoldIsLimited);
    TestHarness::run("repli : maintien sans commande valide", testHoldNeedsValidCommand);
    TestHarness::run("repli : PID sans a-coup", testPIDHoldIsBumpless);
    TestHarness::run("repli : parametres et verrouillage", testFaultParameters);
}
