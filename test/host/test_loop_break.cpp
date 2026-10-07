#include "TestHarness.h"

#include <Measurements/Temperature/Temperature.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <Regulator/LoopBreakAlarm.h>
#include <Regulator/PID.h>
#include <Regulator/Thermostat.h>
#include <hmi/AlarmDisplay.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    using Status = MeasurementStatus;

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

    // Régulateur dont la commande, les butées et le sens sont fixés.
    class LoopRegulator final : public Regulator
    {
    public:
        double_t setpoint = 50.0;
        double_t ti = 0.0;
        int8_t direction = 1;
        bool automatic = true;

        void update(uint32_t) override
        {
        }

        void set(double_t command)
        {
            writeCommand(command);
        }

        bool readSetpoint(double_t& value) const override
        {
            value = setpoint;
            return true;
        }

        int8_t actionDirection() const override
        {
            return direction;
        }

        bool isAutomatic() const override
        {
            return automatic;
        }

        double_t integralTime() const override
        {
            return ti;
        }
    };

    // Alarme active, 100 s de détection, 2 °C de variation.
    void prepare(
        LoopBreakAlarm& alarm,
        ControlledTemperature& temperature,
        LoopRegulator& regulator)
    {
        regulator.begin("regulator");
        alarm.begin("loop", "Boucle", temperature, regulator);
        alarm.settings.enabled = true;
        alarm.settings.detectionTime = 100;
    }

    bool activeAt(
        LoopBreakAlarm& alarm,
        ControlledTemperature& temperature,
        double_t value,
        uint32_t now)
    {
        temperature.set(value);
        alarm.update(now);
        return alarm.isActive();
    }

    void testDetectionTime()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);

        alarm.settings.detectionTime = 0;
        CHECK_TRUE(alarm.detectionTimeMs() == 600000);   // sans Ti

        regulator.ti = 100.0;
        CHECK_TRUE(alarm.detectionTimeMs() == 200000);   // 2 × Ti

        regulator.ti = 10.0;
        CHECK_TRUE(alarm.detectionTimeMs() == 60000);    // au moins 60 s

        alarm.settings.detectionTime = 120;
        CHECK_TRUE(alarm.detectionTimeMs() == 120000);
    }

    void testHighSaturation()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);

        // Chauffage à 100 %, mesure 20 °C sous la consigne, immobile.
        regulator.set(1.0);
        CHECK_FALSE(activeAt(alarm, temperature, 30.0, 0));
        CHECK_FALSE(activeAt(alarm, temperature, 30.5, 99999));
        CHECK_TRUE(activeAt(alarm, temperature, 30.5, 100000));

        // Mise en sécurité : le régulateur est verrouillé.
        CHECK_TRUE(alarm.locksOutputs());
        CHECK_TRUE(regulator.isInterlocked());
        CHECK_FALSE(regulator.isCommandValid());

        // Mémorisée : reste signalée tant qu'elle n'est pas acquittée.
        CHECK_TRUE(activeAt(alarm, temperature, 30.5, 200000));

        alarm.acknowledge();
        CHECK_FALSE(regulator.isInterlocked());
        CHECK_TRUE(regulator.isCommandValid());

        // Nouvelle fenêtre après l'acquittement.
        CHECK_FALSE(activeAt(alarm, temperature, 30.5, 200001));
        CHECK_FALSE(activeAt(alarm, temperature, 30.5, 300000));
        CHECK_TRUE(activeAt(alarm, temperature, 30.5, 300001));
    }

    void testProgressRestartsWindow()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);
        regulator.set(1.0);

        // La mesure monte de 2 °C toutes les 90 s : jamais d'alarme.
        uint32_t now = 0;
        for (double_t value = 20.0; value <= 40.0; value += 2.0)
        {
            CHECK_FALSE(activeAt(alarm, temperature, value, now));
            now += 90000;
        }

        // Montée trop lente (1,5 °C en 100 s) : alarme.
        CHECK_FALSE(activeAt(alarm, temperature, 40.0, now));
        CHECK_TRUE(activeAt(alarm, temperature, 41.5, now + 100000));
    }

    void testNotArmed()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);

        // Près de la consigne : pas de surveillance.
        regulator.set(1.0);
        CHECK_FALSE(activeAt(alarm, temperature, 48.5, 0));
        CHECK_FALSE(activeAt(alarm, temperature, 48.5, 500000));

        // Commande hors butée.
        regulator.set(0.8);
        CHECK_FALSE(activeAt(alarm, temperature, 20.0, 600000));
        CHECK_FALSE(activeAt(alarm, temperature, 20.0, 900000));

        // Manuel : pas de surveillance.
        regulator.set(1.0);
        regulator.automatic = false;
        CHECK_FALSE(activeAt(alarm, temperature, 20.0, 1000000));
        CHECK_FALSE(activeAt(alarm, temperature, 20.0, 2000000));

        // Défaut de sonde : signalé ailleurs, pas ici.
        regulator.automatic = true;
        temperature.fail(Status::Open);
        alarm.update(3000000);
        alarm.update(4000000);
        CHECK_FALSE(alarm.isActive());

        // Désactivée : aucun effet.
        alarm.settings.enabled = false;
        CHECK_FALSE(activeAt(alarm, temperature, 20.0, 5000000));
        CHECK_FALSE(activeAt(alarm, temperature, 20.0, 6000000));
        CHECK_FALSE(regulator.isInterlocked());
    }

    void testLowSaturationAndCooling()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);

        // Chauffage à 0 %, mesure 10 °C au-dessus : contacteur collé.
        regulator.set(0.0);
        CHECK_FALSE(activeAt(alarm, temperature, 60.0, 0));
        CHECK_TRUE(activeAt(alarm, temperature, 60.0, 100000));

        // En refroidissant normalement, pas d'alarme.
        LoopBreakAlarm cooling;
        prepare(cooling, temperature, regulator);
        CHECK_FALSE(activeAt(cooling, temperature, 60.0, 0));
        CHECK_FALSE(activeAt(cooling, temperature, 58.0, 90000));
        CHECK_FALSE(activeAt(cooling, temperature, 56.0, 180000));

        // Froid à 100 %, mesure au-dessus de la consigne, qui ne baisse pas.
        LoopRegulator chiller;
        chiller.direction = -1;
        LoopBreakAlarm coldLoop;
        prepare(coldLoop, temperature, chiller);
        chiller.set(1.0);
        CHECK_FALSE(activeAt(coldLoop, temperature, 60.0, 0));
        CHECK_TRUE(activeAt(coldLoop, temperature, 60.0, 100000));

        // Mais une mesure qui baisse relance la fenêtre.
        LoopBreakAlarm coldOk;
        prepare(coldOk, temperature, chiller);
        CHECK_FALSE(activeAt(coldOk, temperature, 60.0, 0));
        CHECK_FALSE(activeAt(coldOk, temperature, 57.0, 90000));
        CHECK_FALSE(activeAt(coldOk, temperature, 56.0, 180000));
    }

    void testOptions()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);
        regulator.set(1.0);

        // Alarme seule, sans mise en sécurité.
        alarm.settings.safeState = false;
        CHECK_FALSE(activeAt(alarm, temperature, 30.0, 0));
        CHECK_TRUE(activeAt(alarm, temperature, 30.0, 100000));
        CHECK_FALSE(regulator.isInterlocked());
        CHECK_TRUE(regulator.isCommandValid());

        // Sans mémorisation : l'alarme cesse avec sa cause.
        alarm.settings.latching = false;
        CHECK_FALSE(activeAt(alarm, temperature, 49.0, 110000));

        // Manuel : le verrouillage ne s'applique pas.
        alarm.settings.safeState = true;
        alarm.settings.latching = true;
        CHECK_FALSE(activeAt(alarm, temperature, 30.0, 200000));
        CHECK_TRUE(activeAt(alarm, temperature, 30.0, 300000));
        CHECK_TRUE(regulator.isInterlocked());
        regulator.automatic = false;
        CHECK_FALSE(regulator.isInterlocked());
        regulator.automatic = true;
        CHECK_TRUE(regulator.isInterlocked());

        // Un régulateur sans alarme reliée n'est jamais verrouillé.
        LoopRegulator alone;
        alone.begin("alone");
        alone.set(1.0);
        CHECK_FALSE(alone.isInterlocked());
        CHECK_TRUE(alone.isCommandValid());

        Parameter storage[8];
        ParameterList parameters;
        parameters.begin(storage, 8);
        alarm.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());
        CHECK_TRUE(parameters.count() == 5);
        CHECK_TRUE(parameters.find("loop", "safe_state") != nullptr);
    }

    void testPIDInterlock()
    {
        ControlledTemperature temperature;
        PID pid;
        pid.begin("pid", temperature);
        pid.settings.setpoint = 50.0;
        pid.settings.kp = 0.2;
        pid.settings.ti = 20.0;
        pid.settings.td = 0.0;

        LoopBreakAlarm alarm;
        alarm.begin("pid_loop", "Boucle PID", temperature, pid);
        alarm.settings.enabled = true;   // détection auto : 2 × 20 s → 60 s

        ProcessControl process;
        CHECK_TRUE(process.add(temperature));
        CHECK_TRUE(process.add(pid));
        CHECK_TRUE(process.add(alarm));

        // Sonde hors du process : la mesure reste à 20 °C, sortie à 100 %.
        temperature.set(20.0);
        uint32_t now = 0;
        for (; now <= 30000; now += 1000)
            process.updateMeasurementsAndRegulators(now);

        CHECK_NEAR(pid.readCommand(), 1.0, 1e-9);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_FALSE(alarm.isActive());

        for (; now <= 100000; now += 1000)
            process.updateMeasurementsAndRegulators(now);

        CHECK_TRUE(alarm.isActive());
        CHECK_TRUE(pid.isInterlocked());
        CHECK_FALSE(pid.isCommandValid());

        ProcessSnapshot snapshot;
        char text[32];
        uint16_t color = 0;
        process.captureSnapshot(snapshot, now);
        AlarmDisplay::format(snapshot, text, sizeof(text), 18, color);
        CHECK_TRUE(std::strcmp(text, "ALARME Boucle PID") == 0);

        // Acquittement : le PID repart (un cycle de réinitialisation).
        process.acknowledgeAlarms();
        CHECK_FALSE(pid.isInterlocked());
        process.updateMeasurementsAndRegulators(now += 1000);
        process.updateMeasurementsAndRegulators(now += 1000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_FALSE(alarm.isActive());
    }

    void testSafeStateAlwaysLatches()
    {
        // Thermostat de chauffage, résistance grillée : la mesure reste à
        // 30 °C pour 50 °C de consigne. Mise en sécurité sans mémorisation.
        ControlledTemperature temperature;
        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 50.0;

        LoopBreakAlarm alarm;
        alarm.begin("loop", "Boucle", temperature, thermostat);
        alarm.settings.enabled = true;
        alarm.settings.detectionTime = 60;
        alarm.settings.latching = false;
        alarm.settings.safeState = true;

        ProcessControl process;
        CHECK_TRUE(process.add(temperature));
        CHECK_TRUE(process.add(thermostat));
        CHECK_TRUE(process.add(alarm));

        temperature.set(30.0);
        uint32_t now = 0;
        uint32_t safeSeconds = 0;

        for (; now <= 200000; now += 1000)
        {
            process.updateMeasurementsAndRegulators(now);

            if (!thermostat.isCommandValid())
                safeSeconds++;
        }

        // La sécurité tient jusqu'à l'acquittement : plus de chauffe à 100 %
        // entre deux fenêtres de détection.
        CHECK_TRUE(alarm.isActive());
        CHECK_TRUE(alarm.isLatched());
        CHECK_TRUE(thermostat.isInterlocked());
        CHECK_TRUE(safeSeconds >= 135);

        // Acquittement : sécurité levée, surveillance relancée ; la boucle
        // toujours ouverte redéclenche après un nouveau temps de détection.
        process.acknowledgeAlarms();
        CHECK_FALSE(thermostat.isInterlocked());
        process.updateMeasurementsAndRegulators(now += 1000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        for (uint32_t end = now + 61000; now < end; )
            process.updateMeasurementsAndRegulators(now += 1000);

        CHECK_TRUE(thermostat.isInterlocked());

        // Sans mise en sécurité, le réglage Mémorisation s'applique :
        // l'alarme cesse avec sa cause.
        alarm.settings.safeState = false;
        process.acknowledgeAlarms();

        for (uint32_t end = now + 61000; now < end; )
            process.updateMeasurementsAndRegulators(now += 1000);

        CHECK_TRUE(alarm.isActive());
        CHECK_FALSE(thermostat.isInterlocked());
        temperature.set(49.0);
        process.updateMeasurementsAndRegulators(now += 1000);
        CHECK_FALSE(alarm.isActive());
    }

    void testLongSaturation()
    {
        ControlledTemperature temperature;
        LoopRegulator regulator;
        LoopBreakAlarm alarm;
        prepare(alarm, temperature, regulator);
        alarm.settings.safeState = false;
        alarm.settings.latching = false;

        regulator.set(1.0);
        CHECK_FALSE(activeAt(alarm, temperature, 30.0, 0));
        CHECK_TRUE(activeAt(alarm, temperature, 30.0, 100000));

        // Saturation de plus de 49 jours : millis() reboucle, l'alarme reste.
        CHECK_TRUE(activeAt(alarm, temperature, 30.0, 3000000000UL));
        CHECK_TRUE(activeAt(alarm, temperature, 30.0, 5000));
    }
}

void runLoopBreakTests()
{
    TestHarness::run("boucle ouverte : temps de detection", testDetectionTime);
    TestHarness::run("boucle ouverte : butee haute", testHighSaturation);
    TestHarness::run("boucle ouverte : progression", testProgressRestartsWindow);
    TestHarness::run("boucle ouverte : surveillance suspendue", testNotArmed);
    TestHarness::run("boucle ouverte : butee basse et froid", testLowSaturationAndCooling);
    TestHarness::run("boucle ouverte : options", testOptions);
    TestHarness::run("boucle ouverte : verrouillage du PID", testPIDInterlock);
    TestHarness::run("boucle ouverte : mise en sécurité mémorisée", testSafeStateAlwaysLatches);
    TestHarness::run("boucle ouverte : saturation de plus de 49 jours", testLongSaturation);
}
