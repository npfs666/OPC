#include "TestHarness.h"

#include <Hardware/RTC.h>
#include <Measurements/Temperature/Temperature.h>
#include <Regulator/HeatingCurve.h>
#include <Regulator/PID.h>
#include <Regulator/Thermostat.h>
#include <Regulator/TimeSchedule.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    using Status = MeasurementStatus;
    using State = HeatingCurve::State;

    constexpr uint8_t MONDAY = 1;

    class ControlledTemperature final : public Temperature
    {
    public:
        explicit ControlledTemperature(const char* name)
        {
            begin(name);
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

    // Pente de la courbe par défaut : (45 - 20) / (20 - -10).
    constexpr double_t SLOPE = 25.0 / 30.0;

    double_t setpointOf(const Regulator& regulator)
    {
        double_t setpoint = NAN;
        CHECK_TRUE(regulator.readSetpoint(setpoint));
        return setpoint;
    }

    void testCurve()
    {
        ControlledTemperature outdoor("ext");
        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);

        // Points, interpolation, prolongement borné.
        CHECK_NEAR(curve.flowFor(-10.0, 20.0), 45.0, 1e-9);
        CHECK_NEAR(curve.flowFor(20.0, 20.0), 20.0, 1e-9);
        CHECK_NEAR(curve.flowFor(5.0, 20.0), 32.5, 1e-9);
        CHECK_NEAR(curve.flowFor(-20.0, 20.0), 50.0, 1e-9);
        CHECK_NEAR(curve.flowFor(25.0, 20.0), 20.0, 1e-9);

        // Ambiance 21 °C : décalage de (1 + pente) × 1 K.
        CHECK_NEAR(curve.flowFor(5.0, 21.0), 32.5 + 1.0 + SLOPE, 1e-9);

        // Avant la première mesure : état sûr.
        curve.update(1000);
        CHECK_FALSE(curve.isCommandValid());
        CHECK_TRUE(curve.state() == State::Waiting);

        outdoor.set(5.0);
        curve.update(2000);
        CHECK_TRUE(curve.isCommandValid());
        CHECK_NEAR(curve.readCommand(), 1.0, 0.0);
        CHECK_TRUE(curve.state() == State::Comfort);
        CHECK_NEAR(setpointOf(curve), 32.5, 1e-9);

        curve.settings.roomSetpoint = 21.0;
        curve.update(3000);
        CHECK_NEAR(setpointOf(curve), 32.5 + 1.0 + SLOPE, 1e-9);

        // Inhibée par la glue : arrêt commandé, sans consigne.
        curve.inhibit(true);
        curve.update(4000);
        double_t setpoint = 0.0;
        CHECK_TRUE(curve.isCommandValid());
        CHECK_NEAR(curve.readCommand(), 0.0, 0.0);
        CHECK_FALSE(curve.readSetpoint(setpoint));
    }

    void testSummerFrostAndFallback()
    {
        ControlledTemperature outdoor("ext");
        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);

        // Arrêt été à 17 °C, reprise à 16 °C.
        outdoor.set(17.5);
        curve.update(1000);
        double_t setpoint = 0.0;
        CHECK_TRUE(curve.state() == State::Summer);
        CHECK_TRUE(curve.isCommandValid());
        CHECK_NEAR(curve.readCommand(), 0.0, 0.0);
        CHECK_FALSE(curve.readSetpoint(setpoint));

        outdoor.set(16.5);
        curve.update(2000);
        CHECK_TRUE(curve.state() == State::Summer);

        outdoor.set(15.9);
        curve.update(3000);
        CHECK_TRUE(curve.state() == State::Comfort);

        // Sonde coupée : la courbe suit T. ext. secours (0 °C).
        outdoor.fail(Status::Open);
        curve.update(4000);
        CHECK_TRUE(curve.isCommandValid());
        CHECK_TRUE(curve.isOutdoorFallback());
        CHECK_NEAR(setpointOf(curve), 20.0 + SLOPE * 20.0, 1e-9);

        // Acquisition reprise (NotReady) : la dernière valeur filtrée.
        outdoor.fail(Status::NotReady);
        curve.update(5000);
        CHECK_FALSE(curve.isOutdoorFallback());
        CHECK_NEAR(curve.outdoorTemperature(), 15.9, 1e-9);
    }

    void testFilter()
    {
        ControlledTemperature outdoor("ext");
        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);
        curve.settings.buildingTimeConstant = 1;

        outdoor.set(0.0);
        curve.update(0);
        CHECK_NEAR(curve.outdoorTemperature(), 0.0, 0.0);

        // Échelon de 10 K : 63 % après une constante de temps.
        outdoor.set(10.0);

        for (uint32_t second = 10; second <= 3600; second += 10)
            curve.update(second * 1000);

        CHECK_NEAR(curve.outdoorTemperature(), 10.0 * (1.0 - std::exp(-1.0)), 0.05);
    }

    void testSchedule()
    {
        ClockSample clock;
        clock.dateTime.dayOfWeek = MONDAY;
        clock.dateTime.hour = 10;
        clock.valid = true;

        TimeSchedule schedule;
        schedule.begin("prog", "Confort", clock);
        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, 8 * 60, 18 * 60
        };

        ControlledTemperature outdoor("ext");
        outdoor.set(5.0);

        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);
        curve.setSchedule(schedule, 16.0);
        CHECK_TRUE(curve.requiresClock());

        curve.update(1000);
        CHECK_TRUE(curve.state() == State::Comfort);
        CHECK_NEAR(setpointOf(curve), 32.5, 1e-9);

        // Hors plage : ambiance réduite à 16 °C.
        clock.dateTime.hour = 20;
        curve.update(2000);
        CHECK_TRUE(curve.state() == State::Reduced);
        CHECK_NEAR(setpointOf(curve), 32.5 - 4.0 * (1.0 + SLOPE), 1e-9);

        // Hors plage en « Arrêt » : arrêt commandé, ou hors-gel.
        curve.scheduledSetpoint.settings.outside =
            ScheduledSetpoint::Outside::Off;
        curve.update(3000);
        CHECK_TRUE(curve.state() == State::Off);
        CHECK_TRUE(curve.isCommandValid());
        CHECK_NEAR(curve.readCommand(), 0.0, 0.0);

        outdoor.set(-2.0);
        curve.update(4000);
        CHECK_TRUE(curve.state() == State::Frost);
        CHECK_NEAR(curve.readCommand(), 1.0, 0.0);

        // Heure inconnue : état sûr.
        clock.valid = false;
        curve.update(5000);
        CHECK_TRUE(curve.state() == State::ClockInvalid);
        CHECK_FALSE(curve.isCommandValid());
    }

    void testPIDFollowsCurve()
    {
        ControlledTemperature outdoor("ext");
        ControlledTemperature flow("depart");
        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);

        PID pid;
        pid.begin("pid", "Départ", flow);
        pid.setTunings(0.1, 300.0, 0.0);
        pid.followSetpoint(curve);
        CHECK_TRUE(pid.followsSetpoint());

        // Premier cycle : initialisation du PID, commande au suivant.
        outdoor.set(5.0);
        flow.set(30.0);
        curve.update(1000);
        pid.update(1000);
        curve.update(2000);
        pid.update(2000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(setpointOf(pid), 32.5, 1e-9);
        CHECK_TRUE(pid.readCommand() > 0.0);

        // Été : arrêt commandé, commande 0 valide.
        outdoor.set(18.0);
        curve.update(3000);
        pid.update(3000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 0.0);

        // Source invalide : défaut.
        HeatingCurve waiting;
        ControlledTemperature missing("absent");
        waiting.begin("waiting", "Sans mesure", missing);
        PID follower;
        follower.begin("follower", "Suiveur", flow);
        follower.followSetpoint(waiting);
        waiting.update(4000);
        follower.update(4000);
        CHECK_FALSE(follower.isCommandValid());

        // La consigne du PID quitte le menu.
        Parameter storage[40];
        ParameterList list;
        list.begin(storage, 40);
        pid.registerParameters(list);
        CHECK_FALSE(list.hasError());
        CHECK_TRUE(list.find("pid", "setpoint") == nullptr);
        CHECK_TRUE(list.find("pid", "kp") != nullptr);
    }

    void testThermostatFollowsCurve()
    {
        ControlledTemperature outdoor("ext");
        ControlledTemperature flow("depart");
        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);

        Thermostat boiler;
        boiler.begin("boiler", "Chaudière", flow);
        boiler.settings.hysteresis = 4.0;
        boiler.followSetpoint(curve);

        outdoor.set(5.0);
        flow.set(25.0);
        curve.update(1000);
        boiler.update(1000);
        CHECK_TRUE(boiler.isCommandValid());
        CHECK_NEAR(boiler.readCommand(), 1.0, 0.0);
        CHECK_NEAR(setpointOf(boiler), 32.5, 1e-9);

        flow.set(35.0);
        curve.update(2000);
        boiler.update(2000);
        CHECK_NEAR(boiler.readCommand(), 0.0, 0.0);

        flow.set(25.0);
        outdoor.set(18.0);
        curve.update(3000);
        boiler.update(3000);
        CHECK_TRUE(boiler.isCommandValid());
        CHECK_NEAR(boiler.readCommand(), 0.0, 0.0);
    }

    void testMenu()
    {
        ControlledTemperature outdoor("ext");
        HeatingCurve curve;
        curve.begin("curve", "Loi d'eau", outdoor);

        Parameter storage[20];
        ParameterList list;
        list.begin(storage, 20);
        curve.registerParameters(list);
        CHECK_FALSE(list.hasError());
        CHECK_TRUE(list.find("curve", "room_setpoint")->live);

        ParameterEditor editor;
        editor.begin(list);
        editor.capture();
        CHECK_TRUE(curve.validateParameters(editor));

        // Points froid et doux trop proches.
        ParameterDraft& mild = const_cast<ParameterDraft&>(
            *editor.find("curve", "mild_outdoor"));
        mild.numberValue = -8.0;
        CHECK_FALSE(curve.validateParameters(editor));
        mild.numberValue = 20.0;

        ParameterDraft& maxFlow = const_cast<ParameterDraft&>(
            *editor.find("curve", "max_flow"));
        maxFlow.numberValue = 20.0;
        CHECK_FALSE(curve.validateParameters(editor));
    }
}

void runHeatingCurveTests()
{
    TestHarness::run("Loi d'eau courbe et ambiance", testCurve);
    TestHarness::run("Loi d'eau été, hors-gel et secours", testSummerFrostAndFallback);
    TestHarness::run("Loi d'eau filtre extérieur", testFilter);
    TestHarness::run("Loi d'eau programme", testSchedule);
    TestHarness::run("PID consigne d'exécution", testPIDFollowsCurve);
    TestHarness::run("Thermostat consigne d'exécution", testThermostatFollowsCurve);
    TestHarness::run("Loi d'eau menu", testMenu);
}
