#include "TestHarness.h"

#include <Measurements/Temperature/Temperature.h>
#include <Outputs/Actuator.h>
#include <ProcessControl.h>
#include <ProcessLogic.h>
#include <Regulator/Comparator.h>
#include <Regulator/DelayTimer.h>
#include <Regulator/LimitAlarm.h>
#include <Regulator/LoopBreakAlarm.h>
#include <Regulator/PID.h>
#include <Regulator/Thermostat.h>
#include <Regulator/TimeSchedule.h>

#include <cmath>

namespace
{
    using Status = MeasurementStatus;

    constexpr uint8_t MONDAY = 1;

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
    };

    // Thermostat de chauffage 20 °C ± 1 °C, en marche à 18 °C.
    void startHeating(Thermostat& thermostat, ControlledTemperature& temperature)
    {
        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        temperature.set(18.0);
        thermostat.update(0);
    }

    void testThermostat()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        // Inhibé : commande 0, valide (arrêt commandé), sans consigne active.
        thermostat.inhibit(true);
        CHECK_TRUE(thermostat.isInhibited());
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);

        thermostat.update(1000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);

        double_t setpoint = 0.0;
        CHECK_FALSE(thermostat.readSetpoint(setpoint));

        // Levée dans la bande : départ à l'arrêt, comme après le menu.
        thermostat.inhibit(false);
        temperature.set(19.5);
        thermostat.update(2000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
        CHECK_TRUE(thermostat.readSetpoint(setpoint));

        // Sous la bande : chauffe de nouveau.
        temperature.set(18.0);
        thermostat.update(3000);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);
    }

    void testManualFirst()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);
        thermostat.inhibit(true);

        // Le mode manuel passe avant l'inhibition.
        thermostat.settings.operation = Thermostat::Operation::ForcedOn;
        thermostat.update(1000);
        CHECK_TRUE(thermostat.isManual());
        CHECK_FALSE(thermostat.isInhibited());
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        // Retour en Auto : l'inhibition demandée reprend effet.
        thermostat.settings.operation = Thermostat::Operation::Auto;
        thermostat.update(2000);
        CHECK_TRUE(thermostat.isInhibited());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
    }

    void testInterlockFirst()
    {
        // Un verrouillage par alarme passe avant l'inhibition : état sûr.
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);

        LoopBreakAlarm alarm;
        alarm.begin("boucle", "Boucle", temperature, thermostat);
        alarm.settings.enabled = true;
        alarm.settings.detectionTime = 10;
        thermostat.setInterlock(alarm);

        // Chauffe à 100 %, 5 K sous la consigne, sans que la mesure bouge :
        // boucle ouverte.
        temperature.set(15.0);
        thermostat.update(0);
        alarm.update(0);
        alarm.update(10000);
        CHECK_TRUE(alarm.locksOutputs());

        thermostat.inhibit(true);
        CHECK_FALSE(thermostat.isCommandValid());
    }

    void testLoopBreakSuspended()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);

        LoopBreakAlarm alarm;
        alarm.begin("boucle", "Boucle", temperature, thermostat);
        alarm.settings.enabled = true;
        alarm.settings.detectionTime = 10;

        // 5 K sous la consigne. Inhibé pendant toute la fenêtre : pas de
        // boucle ouverte.
        temperature.set(15.0);
        thermostat.update(0);
        alarm.update(0);
        thermostat.inhibit(true);
        thermostat.update(5000);
        alarm.update(5000);
        alarm.update(20000);
        CHECK_FALSE(alarm.isActive());

        // À la levée, la surveillance repart d'une fenêtre neuve.
        thermostat.inhibit(false);
        thermostat.update(21000);
        alarm.update(21000);
        alarm.update(30999);
        CHECK_FALSE(alarm.isActive());
        alarm.update(31000);
        CHECK_TRUE(alarm.isActive());
    }

    void testRelativeAlarmSuspended()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        startHeating(thermostat, temperature);

        // Écart haut de 5 K au-dessus de la consigne (20 °C).
        LimitAlarm alarm;
        alarm.begin("ecart", "Ecart", temperature);
        alarm.settings.enabled = true;
        alarm.settings.type = LimitAlarm::Type::DeviationHigh;
        alarm.settings.limit = 5.0;
        alarm.setReference(thermostat);

        temperature.set(30.0);
        thermostat.update(1000);
        alarm.update(1000);
        CHECK_TRUE(alarm.isActive());

        // Inhibé (dégivrage) : alarme relative suspendue.
        thermostat.inhibit(true);
        thermostat.update(2000);
        alarm.update(2000);
        CHECK_FALSE(alarm.isActive());

        thermostat.inhibit(false);
        thermostat.update(3000);
        alarm.update(3000);
        CHECK_TRUE(alarm.isActive());
    }

    // PID de chauffage en régime établi, avec intégrale.
    void runPID(PID& pid, ControlledTemperature& temperature, uint32_t until)
    {
        for (uint32_t now = 0; now <= until; now += 1000)
            pid.update(now);
        (void)temperature;
    }

    void preparePID(PID& pid, ControlledTemperature& temperature)
    {
        pid.begin("pid", temperature);
        pid.settings.setpoint = 50.0;
        pid.settings.kp = 0.05;
        pid.settings.ti = 100.0;
        pid.settings.td = 0.0;
        temperature.set(45.0);
    }

    void testPIDRestartsLikeEnable()
    {
        ControlledTemperature temperature;
        PID pid;
        preparePID(pid, temperature);
        runPID(pid, temperature, 60000);

        const double_t integrated = pid.readCommand();

        pid.inhibit(true);
        pid.update(61000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 0.0);

        double_t setpoint = 0.0;
        CHECK_FALSE(pid.readSetpoint(setpoint));
        CHECK_FALSE(pid.isAutoTuneActive());

        // Levée : comme une réactivation, d'une intégrale nulle. Même sortie
        // qu'un PID neuf dans les mêmes conditions.
        pid.inhibit(false);
        pid.update(62000);
        pid.update(63000);

        ControlledTemperature fresh;
        PID reference;
        preparePID(reference, fresh);
        reference.update(0);
        reference.update(1000);

        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), reference.readCommand(), 1e-9);
        CHECK_TRUE(pid.readCommand() < integrated);
    }

    void testPIDAutoTune()
    {
        ControlledTemperature temperature;
        PID pid;
        preparePID(pid, temperature);
        pid.update(0);

        // Essai abandonné par l'inhibition ; pas de nouvel essai inhibé.
        CHECK_TRUE(pid.startAutoTune(1000));
        CHECK_TRUE(pid.isAutoTuneActive());

        pid.inhibit(true);
        pid.update(2000);
        CHECK_FALSE(pid.isAutoTuneActive());
        CHECK_FALSE(pid.settings.enabled);
        CHECK_FALSE(pid.startAutoTune(3000));

        // Le manuel passe avant l'inhibition.
        pid.settings.operation = PID::Operation::Manual;
        pid.settings.manualOutput = 40.0;
        pid.update(4000);
        CHECK_NEAR(pid.readCommand(), 0.40, 1e-9);
    }

    void testSchedule()
    {
        ClockSample clock;
        clock.dateTime.dayOfWeek = MONDAY;
        clock.dateTime.hour = 12;
        clock.valid = true;

        TimeSchedule schedule;
        schedule.begin("prog", "Programme", clock);
        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, 8 * 60, 18 * 60
        };

        schedule.update(0);
        CHECK_NEAR(schedule.readCommand(), 1.0, 0.0);

        schedule.inhibit(true);
        schedule.update(1000);
        CHECK_TRUE(schedule.isCommandValid());
        CHECK_NEAR(schedule.readCommand(), 0.0, 0.0);

        // Dérogation (marche forcée) : prioritaire.
        schedule.settings.mode = TimeSchedule::Mode::ForcedOn;
        schedule.update(2000);
        CHECK_NEAR(schedule.readCommand(), 1.0, 0.0);

        schedule.settings.mode = TimeSchedule::Mode::Auto;
        schedule.inhibit(false);
        schedule.update(3000);
        CHECK_NEAR(schedule.readCommand(), 1.0, 0.0);
    }

    void testComparatorAndTimer()
    {
        ControlledTemperature temperature;
        Comparator comparator;
        comparator.begin(
            "seuil", "Seuil", temperature,
            Comparator::Direction::Above, 50.0, 45.0);

        temperature.set(55.0);
        comparator.update(0);
        CHECK_TRUE(comparator.isOn());

        // Inhibé : à l'arrêt ; à la levée dans la bande, repart de l'arrêt.
        comparator.inhibit(true);
        comparator.update(1000);
        CHECK_FALSE(comparator.isOn());
        CHECK_TRUE(comparator.isCommandValid());

        comparator.inhibit(false);
        temperature.set(47.0);
        comparator.update(2000);
        CHECK_FALSE(comparator.isOn());

        // Temporisation inhibée : remise à zéro.
        DelayTimer timer;
        timer.begin("retard", "Retard", DelayTimer::Mode::OnDelay, 10);
        CHECK_FALSE(timer.run(true, 0));

        timer.inhibit(true);
        CHECK_FALSE(timer.run(true, 5000));
        CHECK_FALSE(timer.isOn());

        timer.inhibit(false);
        CHECK_FALSE(timer.run(true, 6000));
        CHECK_FALSE(timer.run(true, 15999));
        CHECK_TRUE(timer.run(true, 16000));
    }

    // Glue qui inhibe un thermostat ; l'actionneur lit sa commande.
    class InhibitLogic final : public ProcessLogic
    {
    public:
        Thermostat* thermostat = nullptr;
        bool inhibit = false;

        void processLogic(uint32_t now) override
        {
            (void)now;
            thermostat->inhibit(inhibit);
        }

        void resumeLogic(uint32_t now) override
        {
            (void)now;
        }
    };

    class RecordingActuator final : public Actuator
    {
    public:
        double_t seen = -1.0;
        bool seenValid = false;

        void update(uint32_t now) override
        {
            (void)now;
            seenValid = regulator->isCommandValid();
            seen = regulator->readCommand();
        }
    };

    void testImmediateEffect()
    {
        ControlledTemperature temperature;
        temperature.set(18.0);

        ProcessControl process;
        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        RecordingActuator actuator;
        actuator.begin("actionneur", thermostat);

        InhibitLogic logic;
        logic.thermostat = &thermostat;

        process.add(thermostat);
        process.add(actuator);
        process.setLogic(logic);

        process.updateMeasurementsAndRegulators(1000);
        CHECK_NEAR(actuator.seen, 1.0, 0.0);

        // Inhibé par la glue : l'actionneur voit 0 dès ce cycle.
        logic.inhibit = true;
        process.updateMeasurementsAndRegulators(2000);
        CHECK_TRUE(actuator.seenValid);
        CHECK_NEAR(actuator.seen, 0.0, 0.0);

        // L'inhibition survit à une reprise (menu) : pas de redémarrage avant
        // que la glue ne la réécrive.
        process.resume(3000);
        process.updateMeasurementsAndRegulators(4000);
        CHECK_TRUE(actuator.seenValid);
        CHECK_NEAR(actuator.seen, 0.0, 0.0);
    }
}

void runInhibitTests()
{
    TestHarness::run("inhibition : thermostat", testThermostat);
    TestHarness::run("inhibition : manuel prioritaire", testManualFirst);
    TestHarness::run("inhibition : verrouillage prioritaire", testInterlockFirst);
    TestHarness::run("inhibition : boucle ouverte suspendue", testLoopBreakSuspended);
    TestHarness::run("inhibition : alarme relative suspendue", testRelativeAlarmSuspended);
    TestHarness::run("inhibition : PID repart comme une réactivation", testPIDRestartsLikeEnable);
    TestHarness::run("inhibition : PID et autotune", testPIDAutoTune);
    TestHarness::run("inhibition : programme horaire", testSchedule);
    TestHarness::run("inhibition : comparateur et temporisation", testComparatorAndTimer);
    TestHarness::run("inhibition : effet immédiat et reprise", testImmediateEffect);
}
