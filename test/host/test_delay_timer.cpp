#include "TestHarness.h"

#include <Hardware/RTC.h>
#include <Measurements/Temperature/Temperature.h>
#include <Regulator/DelayTimer.h>
#include <Regulator/PID.h>
#include <Regulator/Thermostat.h>
#include <Regulator/TimeSchedule.h>
#include <hmi/ParameterList.h>

#include <cstdint>
#include <cstring>

namespace
{
    using Mode = DelayTimer::Mode;
    using Unit = DelayTimer::Unit;

    // Régulateur dont la commande est fixée par le test.
    class TestSource final : public Regulator
    {
    public:
        bool valid = true;
        bool on = false;

        void update(uint32_t now) override
        {
            (void)now;

            if (valid)
                writeCommand(on ? 1.0 : 0.0);
            else
                invalidateCommand();
        }
    };

    void testOnDelay()
    {
        DelayTimer timer;
        timer.begin("retard", "Retard", Mode::OnDelay, 10);

        CHECK_FALSE(timer.run(false, 0));
        CHECK_TRUE(timer.isCommandValid());

        // Entrée vraie à 1 s : sortie à 11 s.
        CHECK_FALSE(timer.run(true, 1000));
        CHECK_TRUE(timer.remainingMs(1000) == 10000);
        CHECK_FALSE(timer.run(true, 10999));
        CHECK_TRUE(timer.remainingMs(10999) == 1);
        CHECK_TRUE(timer.run(true, 11000));
        CHECK_TRUE(timer.isOn());
        CHECK_NEAR(timer.readCommand(), 1.0, 0.0);
        CHECK_TRUE(timer.remainingMs(11000) == 0);

        // Entrée fausse : sortie coupée, délai remis à zéro.
        CHECK_FALSE(timer.run(false, 12000));
        CHECK_FALSE(timer.run(true, 13000));
        CHECK_FALSE(timer.run(true, 22999));
        CHECK_TRUE(timer.run(true, 23000));
    }

    void testOffDelay()
    {
        // Post-circulation de 3 min.
        DelayTimer timer;
        timer.begin(
            "post", "Post-circulation", Mode::OffDelay, 3, Unit::Minutes);

        // Jamais en marche : pas de post-circulation au démarrage.
        CHECK_FALSE(timer.run(false, 0));

        CHECK_TRUE(timer.run(true, 1000));

        // Retombée à 2 s : encore 3 min.
        CHECK_TRUE(timer.run(false, 2000));
        CHECK_TRUE(timer.remainingMs(2000) == 180000);
        CHECK_TRUE(timer.run(false, 181999));
        CHECK_FALSE(timer.run(false, 182000));
        CHECK_TRUE(timer.remainingMs(182000) == 0);

        // Nouvelle marche pendant le délai : il repart à la retombée.
        CHECK_TRUE(timer.run(true, 200000));
        CHECK_TRUE(timer.run(false, 201000));
        CHECK_TRUE(timer.run(true, 202000));
        CHECK_TRUE(timer.run(false, 203000));
        CHECK_TRUE(timer.run(false, 382999));
        CHECK_FALSE(timer.run(false, 383000));
    }

    void testZeroDelay()
    {
        DelayTimer onDelay;
        onDelay.begin("a", "A", Mode::OnDelay, 0);
        CHECK_TRUE(onDelay.run(true, 0));
        CHECK_FALSE(onDelay.run(false, 1));

        DelayTimer offDelay;
        offDelay.begin("b", "B", Mode::OffDelay, 0);
        CHECK_TRUE(offDelay.run(true, 0));
        CHECK_FALSE(offDelay.run(false, 1));
    }

    void testWraparound()
    {
        // Délai à cheval sur le débordement de millis().
        const uint32_t start = UINT32_MAX - 2000;

        DelayTimer timer;
        timer.begin("retard", "Retard", Mode::OnDelay, 5);
        CHECK_FALSE(timer.run(true, start));
        CHECK_FALSE(timer.run(true, start + 4999));
        CHECK_TRUE(timer.run(true, start + 5000));

        // Sortie active pendant plus de 49 jours : elle le reste
        // (sommes calculées sur 32 bits, comme millis()).
        CHECK_TRUE(timer.run(true, start + static_cast<uint32_t>(3000000000UL)));
        CHECK_TRUE(timer.run(true, start + static_cast<uint32_t>(4294000000UL)));
        CHECK_TRUE(timer.run(true, start + 5000));

        // Retard à la descente terminé depuis longtemps : il le reste.
        DelayTimer post;
        post.begin("post", "Post", Mode::OffDelay, 5);
        post.run(true, 0);
        post.run(false, 1000);
        CHECK_FALSE(post.run(false, 6000));
        CHECK_FALSE(post.run(false, 3000000000UL));
        CHECK_FALSE(post.run(false, 2000));
    }

    void testResumeKeepsTiming()
    {
        // Intervalle de 6 h : une pause du menu ne le repousse pas.
        DelayTimer interval;
        interval.begin(
            "intervalle", "Intervalle", Mode::OnDelay, 6, Unit::Hours);

        CHECK_FALSE(interval.run(true, 0));
        interval.resume(3600000UL);
        CHECK_FALSE(interval.isCommandValid());
        CHECK_FALSE(interval.isOn());

        CHECK_FALSE(interval.run(true, 21599999UL));
        CHECK_TRUE(interval.run(true, 21600000UL));

        // reset() repart de zéro.
        interval.reset();
        CHECK_FALSE(interval.run(true, 21601000UL));
        CHECK_TRUE(interval.remainingMs(21601000UL) == 21600000UL);
    }

    void testSource()
    {
        // Post-circulation reliée à un régulateur, sans glue.
        TestSource source;
        source.begin("source");

        DelayTimer timer;
        timer.begin("post", "Post", Mode::OffDelay, 10);
        timer.setSource(source);

        source.on = true;
        source.update(0);
        timer.update(0);
        CHECK_TRUE(timer.isOn());

        source.on = false;
        source.update(1000);
        timer.update(1000);
        CHECK_TRUE(timer.isOn());

        // Source invalide : temporisation invalide et remise à zéro.
        source.valid = false;
        source.update(2000);
        timer.update(2000);
        CHECK_FALSE(timer.isCommandValid());
        CHECK_FALSE(timer.isOn());

        source.valid = true;
        source.update(3000);
        timer.update(3000);
        CHECK_TRUE(timer.isCommandValid());
        CHECK_FALSE(timer.isOn());

        // Sans source, update() ne touche pas à la sortie de la glue.
        DelayTimer glue;
        glue.begin("glue", "Glue", Mode::OnDelay, 0);
        glue.run(true, 0);
        glue.update(1000);
        CHECK_TRUE(glue.isOn());
    }

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
            setStatus(MeasurementStatus::Ok);
        }
    };

    void testPostCirculationAfterScheduledStop()
    {
        // Chauffe programmée 06:00-08:00, arrêt hors plage, post-circulation
        // de 3 min reliée au thermostat.
        ControlledTemperature temperature;
        ClockSample clock;
        clock.valid = true;
        clock.dateTime.dayOfWeek = 1;
        clock.dateTime.hour = 7;

        TimeSchedule schedule;
        schedule.begin("prog", "Chauffe", clock);
        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, 6 * 60, 8 * 60
        };

        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 20.0;
        thermostat.setSchedule(schedule, 16.0);
        thermostat.scheduledSetpoint.settings.outside =
            ScheduledSetpoint::Outside::Off;

        DelayTimer postCirculation;
        postCirculation.begin(
            "post", "Post-circ.", Mode::OffDelay, 3, Unit::Minutes);
        postCirculation.setSource(thermostat);

        auto cycle = [&](uint32_t now)
        {
            thermostat.update(now);
            postCirculation.update(now);
        };

        temperature.set(15.0);
        cycle(0);
        CHECK_TRUE(postCirculation.isOn());

        // Fin de plage : le thermostat s'arrête sur ordre, le circulateur
        // tourne encore 3 min.
        clock.dateTime.hour = 8;
        cycle(1000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
        CHECK_TRUE(postCirculation.isOn());

        cycle(180999);
        CHECK_TRUE(postCirculation.isOn());
        cycle(181000);
        CHECK_FALSE(postCirculation.isOn());
        CHECK_TRUE(postCirculation.isCommandValid());

        // Heure inconnue : défaut, la temporisation passe en état sûr.
        clock.valid = false;
        cycle(182000);
        CHECK_FALSE(thermostat.isCommandValid());
        CHECK_FALSE(postCirculation.isCommandValid());

        // PID désactivé : arrêt commandé, commande 0 valide.
        PID pid;
        pid.begin("pid", temperature);
        pid.stop();
        pid.update(0);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 0.0);
    }

    void testParameters()
    {
        Parameter storage[4];
        ParameterList list;

        DelayTimer minutes;
        minutes.begin("egouttage", "Egouttage", Mode::OnDelay, 2, Unit::Minutes);
        minutes.setLabel("Durée");

        list.begin(storage, 4);
        minutes.registerParameters(list);
        CHECK_FALSE(list.hasError());

        const Parameter* delay = list.find("egouttage", "delay");
        CHECK_TRUE(delay != nullptr);
        CHECK_TRUE(delay != nullptr && std::strcmp(delay->name, "Durée") == 0);
        CHECK_TRUE(
            delay != nullptr &&
            std::strcmp(delay->data.integer.unit, "min") == 0);
        CHECK_TRUE(delay != nullptr && delay->data.integer.maximum == 1440);

        // Maximum borné : le délai tient en millisecondes sur 31 bits.
        DelayTimer hours;
        hours.begin("long", "Long", Mode::OnDelay, 1, Unit::Hours);
        hours.setMaximum(10000);
        list.begin(storage, 4);
        hours.registerParameters(list);
        const Parameter* longDelay = list.find("long", "delay");
        CHECK_TRUE(
            longDelay != nullptr &&
            longDelay->data.integer.maximum == INT32_MAX / 3600000);

        // Délai par défaut au-delà du maximum : erreur d'enregistrement.
        DelayTimer tooLong;
        tooLong.begin("trop", "Trop", Mode::OnDelay, 5000);
        list.begin(storage, 4);
        tooLong.registerParameters(list);
        CHECK_TRUE(list.hasError());
    }
}

void runDelayTimerTests()
{
    TestHarness::run("temporisation : retard à la montée", testOnDelay);
    TestHarness::run("temporisation : retard à la descente", testOffDelay);
    TestHarness::run("temporisation : délai nul", testZeroDelay);
    TestHarness::run("temporisation : débordement de millis()", testWraparound);
    TestHarness::run("temporisation : reprise", testResumeKeepsTiming);
    TestHarness::run("temporisation : source", testSource);
    TestHarness::run(
        "temporisation : post-circulation après un arrêt programmé",
        testPostCirculationAfterScheduledStop);
    TestHarness::run("temporisation : paramètres", testParameters);
}
