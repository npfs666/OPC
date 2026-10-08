#include "TestHarness.h"

#include <Hardware/pinout.h>
#include <Outputs/PWMOutput.h>
#include <Outputs/RelayOutput.h>
#include <Outputs/ThreePointActuator.h>
#include <ProcessControl.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr uint32_t STEP_MS = 50;
    constexpr uint32_t TRAVEL_S = 60;

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

        void fail()
        {
            invalidateCommand();
        }
    };

    bool isOn(const Output& output)
    {
        return output.appliedCommand() >= 0.5;
    }

    // Relais qui signale toute mise à l'état sûr ON pendant que l'autre
    // sortie de la vanne est encore alimentée.
    class SpyRelay final : public RelayOutput
    {
    public:
        const Output* other = nullptr;
        bool bothOnSeen = false;

        void forceSafe() override
        {
            RelayOutput::forceSafe();

            if (other != nullptr && isOn(*this) && isOn(*other))
                bothOnSeen = true;
        }
    };

    /*
     * Vanne simulée : sa position réelle suit les sorties appliquées, en
     * butée sur ses fins de course.
     */
    struct Bench
    {
        FixedRegulator regulator;
        ThreePointActuator valve;
        SpyRelay first;     // Ouvrir ou Marche
        SpyRelay second;    // Fermer ou Sens
        ProcessControl process;

        double_t actual = 0.5;
        bool runDirection = false;
        bool interlockBroken = false;

        void begin(
            bool wiredRunDirection,
            bool fallbackOn,
            bool connectSecondFirst = false)
        {
            FakeTime::milliseconds = 1000;
            runDirection = wiredRunDirection;

            regulator.begin("regulator");

            if (runDirection)
            {
                valve.beginRunDirection(
                    "valve", "Vanne", regulator, first, second, TRAVEL_S);
            }
            else
            {
                valve.begin(
                    "valve", "Vanne", regulator, first, second, TRAVEL_S);
            }

            // Repli choisi par Fermer (OpenClose) ou Marche (RunDirection).
            // La sortie verrouillée reçoit un état sûr ON que le verrou annule.
            first.begin(
                "first", "Sortie 1", Board::Rp2040::OUTPUT_1, true,
                runDirection ? fallbackOn : true);
            second.begin(
                "second", "Sortie 2", Board::Rp2040::OUTPUT_2, true,
                runDirection ? true : fallbackOn);

            first.other = &second;
            second.other = &first;

            CHECK_TRUE(process.add(regulator));
            CHECK_TRUE(process.add(valve));

            if (connectSecondFirst)
            {
                CHECK_TRUE(process.connect(valve, second));
                CHECK_TRUE(process.connect(valve, first));
            }
            else
            {
                CHECK_TRUE(process.connect(valve, first));
                CHECK_TRUE(process.connect(valve, second));
            }

            CHECK_TRUE(process.beginOutputs());
            process.resume(millis());
        }

        int8_t physicalMotion() const
        {
            if (runDirection)
                return isOn(first) ? (isOn(second) ? 1 : -1) : 0;

            if (isOn(first) && isOn(second))
                return 0;

            return isOn(first) ? 1 : isOn(second) ? -1 : 0;
        }

        // Fait tourner la boucle de contrôle pendant milliseconds.
        void run(uint32_t milliseconds)
        {
            const uint32_t end = millis() + milliseconds;

            while (millis() < end)
            {
                const bool runWasOn = isOn(first);
                const bool directionWas = isOn(second);

                process.poll(millis());

                if (runDirection)
                {
                    // Sens ne bascule jamais moteur alimenté.
                    if (runWasOn && isOn(first) &&
                        directionWas != isOn(second))
                    {
                        interlockBroken = true;
                    }
                }
                else if (isOn(first) && isOn(second))
                {
                    interlockBroken = true;
                }

                FakeTime::milliseconds += STEP_MS;

                actual +=
                    physicalMotion() * (STEP_MS / (TRAVEL_S * 1000.0));
                actual = actual < 0.0 ? 0.0 : actual > 1.0 ? 1.0 : actual;
            }
        }

        bool safe() const
        {
            return
                !interlockBroken &&
                !first.bothOnSeen &&
                !second.bothOnSeen;
        }
    };

    void testCalibrationAndTracking()
    {
        Bench bench;
        bench.begin(false, true);
        bench.actual = 0.6;

        // Position inconnue : course complète + sur-course vers la fermeture.
        bench.regulator.set(0.3);
        bench.run(1000);
        CHECK_TRUE(isOn(bench.second));
        CHECK_FALSE(isOn(bench.first));
        CHECK_FALSE(bench.valve.isPositionKnown());

        bench.run(70000);
        CHECK_FALSE(bench.valve.isPositionKnown());
        bench.run(2000);
        CHECK_TRUE(bench.valve.isPositionKnown());
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);
        CHECK_TRUE(bench.valve.position() < 0.02);

        // Puis la commande.
        bench.run(25000);
        CHECK_FALSE(isOn(bench.first));
        CHECK_FALSE(isOn(bench.second));
        CHECK_NEAR(bench.valve.position(), 0.3, 0.002);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);

        // Zone morte : 1 % d'écart ne bouge pas.
        bench.regulator.set(0.31);
        bench.run(5000);
        CHECK_FALSE(isOn(bench.first));
        CHECK_NEAR(bench.valve.position(), 0.3, 0.002);

        bench.regulator.set(0.5);
        bench.run(15000);
        CHECK_NEAR(bench.valve.position(), 0.5, 0.002);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);

        CHECK_TRUE(bench.safe());
    }

    void testReversalPause()
    {
        Bench bench;
        bench.begin(false, true);
        bench.actual = 0.0;
        bench.regulator.set(0.0);
        bench.run(75000);
        CHECK_TRUE(bench.valve.isPositionKnown());

        bench.regulator.set(0.9);
        bench.run(5000);
        CHECK_TRUE(isOn(bench.first));

        // Inversion en marche : arrêt, pause, puis l'autre sens.
        bench.regulator.set(0.0);
        bench.run(STEP_MS);
        CHECK_FALSE(isOn(bench.first));
        CHECK_FALSE(isOn(bench.second));
        bench.run(400);
        CHECK_FALSE(isOn(bench.second));
        bench.run(300);
        CHECK_TRUE(isOn(bench.second));

        // Fermeture franche : butée, sur-course (20 % de 60 s), arrêt.
        bench.run(5000);
        CHECK_NEAR(bench.valve.position(), 0.0, 0.0);
        CHECK_TRUE(isOn(bench.second));
        bench.run(12500);
        CHECK_FALSE(isOn(bench.second));
        bench.run(30000);
        CHECK_FALSE(isOn(bench.second));
        CHECK_NEAR(bench.actual, 0.0, 0.0);

        CHECK_TRUE(bench.safe());
    }

    void testFallbackAndResume()
    {
        // Fermer enregistré en premier : le repli doit couper Ouvrir avant.
        Bench bench;
        bench.begin(false, true, true);
        CHECK_NEAR(bench.first.safeCommand(), 0.0, 0.0);
        CHECK_NEAR(bench.second.safeCommand(), 1.0, 0.0);

        bench.actual = 0.0;
        bench.regulator.set(0.0);
        bench.run(75000);
        bench.regulator.set(1.0);
        bench.run(30000);
        CHECK_TRUE(isOn(bench.first));

        bench.process.forceSafeOutputs();
        CHECK_FALSE(isOn(bench.first));
        CHECK_TRUE(isOn(bench.second));

        // Repli prolongé (timeout mesure) : la reprise compte la fermeture.
        const double_t before = bench.valve.position();
        CHECK_TRUE(before > 0.4);
        FakeTime::milliseconds += 80000;
        bench.process.resume(millis());
        CHECK_NEAR(bench.valve.position(), 0.0, 0.0);

        // Commande invalide : même repli, par l'actionneur.
        bench.regulator.set(0.5);
        bench.run(40000);
        CHECK_NEAR(bench.valve.position(), 0.5, 0.01);
        bench.regulator.fail();
        bench.run(1000);
        CHECK_FALSE(isOn(bench.first));
        CHECK_TRUE(isOn(bench.second));

        CHECK_TRUE(bench.safe());
    }

    void testFrozenFallback()
    {
        Bench bench;
        bench.begin(false, false);
        bench.regulator.set(0.0);
        bench.run(75000);
        bench.regulator.set(0.5);
        bench.run(35000);

        bench.regulator.fail();
        bench.run(10000);
        CHECK_FALSE(isOn(bench.first));
        CHECK_FALSE(isOn(bench.second));
        CHECK_NEAR(bench.valve.position(), 0.5, 0.01);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);
    }

    void testRunDirection()
    {
        Bench bench;
        bench.begin(true, true);
        CHECK_NEAR(bench.second.safeCommand(), 0.0, 0.0);
        bench.actual = 0.2;

        // Démarrage en repli (Marche ON : fermeture). Recalage vers
        // l'ouverture : arrêt, pause d'inversion, Sens, puis Marche.
        CHECK_TRUE(isOn(bench.first));
        bench.regulator.set(0.8);
        bench.run(STEP_MS);
        CHECK_FALSE(isOn(bench.first));
        CHECK_FALSE(isOn(bench.second));
        bench.run(500);
        CHECK_FALSE(isOn(bench.second));
        bench.run(100);
        CHECK_TRUE(isOn(bench.second));
        CHECK_FALSE(isOn(bench.first));
        bench.run(200);
        CHECK_TRUE(isOn(bench.first));

        bench.run(75000);
        CHECK_TRUE(bench.valve.isPositionKnown());
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);
        bench.run(15000);
        CHECK_NEAR(bench.valve.position(), 0.8, 0.002);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);

        // Fermeture : Sens bascule Marche coupée.
        bench.regulator.set(0.2);
        bench.run(40000);
        CHECK_FALSE(isOn(bench.second));
        CHECK_NEAR(bench.valve.position(), 0.2, 0.002);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);

        // Repli : Sens au repos (fermer), Marche selon son état sûr.
        bench.regulator.set(0.6);
        bench.run(10000);
        CHECK_TRUE(isOn(bench.first));
        CHECK_TRUE(isOn(bench.second));
        CHECK_TRUE(bench.safe());

        // Le repli inverse sans pause (accepté, sans court-circuit possible) :
        // hors de la vérification de run().
        bench.regulator.fail();
        bench.process.poll(millis());
        CHECK_TRUE(isOn(bench.first));
        CHECK_FALSE(isOn(bench.second));
    }

    void testMinimumTimes()
    {
        // Les temps minimaux allongent les impulsions ; l'estimation suit
        // l'état réellement appliqué.
        Bench bench;
        bench.first.settings.minOnTime = 5;
        bench.begin(false, true);

        bench.actual = 0.0;
        bench.regulator.set(0.0);
        bench.run(75000);

        // 3 s demandées, 5 s appliquées, puis retour par Fermer.
        bench.regulator.set(0.05);
        bench.run(5200);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);
        CHECK_TRUE(bench.valve.position() > 0.08);
        bench.run(10000);
        CHECK_NEAR(bench.actual, bench.valve.position(), 0.002);
        CHECK_NEAR(bench.valve.position(), 0.05, 0.02);
        CHECK_TRUE(bench.safe());
    }

    void testConnectAndMenu()
    {
        FixedRegulator regulator;
        regulator.begin("regulator");
        ThreePointActuator valve;
        PWMOutput open, close, extra;
        PWMOutput::sharedSettings.frequency = 1000;
        valve.begin("valve", "Vanne", regulator, open, close, 90);
        open.begin("open", "Ouvrir", Board::Rp2040::OUTPUT_3, true, 1.0);
        close.begin("close", "Fermer", Board::Rp2040::OUTPUT_4, true, 0.0);

        ProcessControl process;
        CHECK_TRUE(process.add(valve));
        CHECK_FALSE(process.connect(valve, extra));
        CHECK_TRUE(process.connect(valve, open));
        CHECK_TRUE(process.connect(valve, close));
        CHECK_TRUE(process.beginOutputs());
        CHECK_NEAR(open.safeCommand(), 0.0, 0.0);

        Parameter storage[16];
        ParameterList parameters;
        parameters.begin(storage, 16);
        valve.registerParameters(parameters);
        open.registerParameters(parameters);
        close.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());
        CHECK_TRUE(parameters.find("valve", "travel_time") != nullptr);

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();
        CHECK_TRUE(valve.validateParameters(editor));

        // Fermer en PWM : repli à 0 ou 1 seulement.
        ParameterDraft& safeCommand =
            const_cast<ParameterDraft&>(*editor.find("close", "safe_command"));
        safeCommand.numberValue = 0.5;
        CHECK_FALSE(valve.validateParameters(editor));
        safeCommand.numberValue = 1.0;
        CHECK_TRUE(valve.validateParameters(editor));
    }
}

void runThreePointTests()
{
    TestHarness::run("Vanne 3 points recalage et poursuite", testCalibrationAndTracking);
    TestHarness::run("Vanne 3 points inversion et butée", testReversalPause);
    TestHarness::run("Vanne 3 points repli fermeture et reprise", testFallbackAndResume);
    TestHarness::run("Vanne 3 points repli figé", testFrozenFallback);
    TestHarness::run("Vanne 3 points câblage Marche / Sens", testRunDirection);
    TestHarness::run("Vanne 3 points temps minimaux du relais", testMinimumTimes);
    TestHarness::run("Vanne 3 points raccordement et menu", testConnectAndMenu);
}
