#include "TemplateBench.h"

#include <Hardware/pinout.h>
#include <Templates/HeatingCircuitInstallation.h>
#include <hardware/gpio.h>

/*
 * Template circuit de chauffage complet sur l'hôte : départ (Pt100) et
 * extérieure (Pt1000) simulées par leur résistance, vanne lue sur les
 * relais 1 (ouvrir) et 2 (fermer), pompe sur la sortie DC 1.
 *
 * Avec la courbe par défaut, 5 °C dehors donne 32,5 °C de départ en confort
 * (20 °C) et 27 °C en réduit (17 °C).
 */
namespace
{
    // Ordre d'addSensor() dans le template.
    constexpr size_t FLOW = 0;
    constexpr size_t OUTDOOR = 1;

    constexpr uint8_t MONDAY = 1;

    struct HeatingBench : TemplateBench<HeatingCircuitInstallation>
    {
        void setOutdoor(double_t celsius)
        {
            // Pt1000 : dix fois la résistance d'une Pt100.
            board.sensorResistances[OUTDOOR] =
                10.0 * PT100::getTemperatureToResistance(celsius);
        }

        bool opening() const
        {
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_1] == HIGH;
        }

        bool closing() const
        {
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_2] == HIGH;
        }

        bool pumpOn() const
        {
            return FakeGPIO::levels[Board::Rp2040::OUTPUT_3];
        }

        // Démarrage à midi (confort), 5 °C dehors, recalage de la vanne fait.
        void start(double_t flow)
        {
            CHECK_TRUE(started);
            setClock(MONDAY, 12);
            setOutdoor(5.0);
            setTemperature(FLOW, flow);
            advance(150);
        }

        // La vanne a bougé dans ce sens pendant la durée donnée.
        bool movesDuring(uint32_t seconds, bool open)
        {
            for (uint32_t i = 0; i < seconds; i++)
            {
                cycle();

                if (open ? opening() : closing())
                    return true;
            }

            return false;
        }
    };

    void testStartupAndRegulation()
    {
        HeatingBench bench;
        CHECK_TRUE(bench.started);
        bench.setClock(MONDAY, 12);
        bench.setOutdoor(5.0);
        bench.setTemperature(FLOW, 25.0);

        // Demande de chauffe : la pompe tourne. Vanne de position inconnue :
        // recalage par une fermeture complète (PID sous 50 %).
        bench.advance(5);
        CHECK_TRUE(bench.pumpOn());
        CHECK_TRUE(bench.closing());
        CHECK_FALSE(bench.opening());

        // Course de 120 s + 20 % de sur-course, puis ouverture : le départ
        // est sous la consigne.
        bench.advance(150);
        CHECK_FALSE(bench.closing());
        CHECK_TRUE(bench.movesDuring(60, true));

        // Départ au-dessus de la consigne : la vanne se referme.
        bench.setTemperature(FLOW, 36.0);
        CHECK_TRUE(bench.movesDuring(600, false));
    }

    void testSchedule()
    {
        // 29 °C : sous le départ confort (32,5), au-dessus du réduit (27).
        HeatingBench bench;
        bench.start(29.0);
        CHECK_TRUE(bench.movesDuring(120, true));

        // Nuit : ambiance réduite, la vanne se referme ; la pompe tourne.
        bench.setClock(MONDAY, 23);
        CHECK_TRUE(bench.movesDuring(900, false));
        CHECK_TRUE(bench.pumpOn());

        // Heure inconnue : état sûr, la vanne se ferme.
        bench.setClock(MONDAY, 23, false);
        bench.advance(2);
        CHECK_TRUE(bench.closing());
        CHECK_FALSE(bench.opening());
    }

    void testHomeSetpoint()
    {
        HeatingBench bench;

        // Consigne d'ambiance à l'écran d'accueil, appliquée sans pause.
        const Parameter* room = bench.parameter("heating_curve", "room_setpoint");
        CHECK_TRUE(room != nullptr && room->live);
        CHECK_TRUE(bench.installation.homeSetpoint() == room);

        // La consigne du PID n'est pas un réglage : elle suit la courbe.
        CHECK_TRUE(bench.parameter("heating_flow", "setpoint") == nullptr);

        // 34 °C : au-dessus du départ à 20 °C d'ambiance, sous celui à 22 °C
        // (32,5 + 2 × 1,83 = 36,2 °C).
        bench.start(34.0);
        CHECK_FALSE(bench.movesDuring(300, true));
        bench.setNumber("heating_curve", "room_setpoint", 22.0);
        CHECK_TRUE(bench.movesDuring(900, true));
    }

    void testSummerAndOverrun()
    {
        HeatingBench bench;
        bench.start(30.0);
        CHECK_TRUE(bench.pumpOn());

        // Arrêt été : la vanne se ferme, la pompe post-circule 5 min.
        bench.setOutdoor(19.0);
        bench.advance(30);
        CHECK_TRUE(bench.closing());
        CHECK_TRUE(bench.pumpOn());
        bench.advance(260);
        CHECK_TRUE(bench.pumpOn());
        bench.advance(20);
        CHECK_FALSE(bench.pumpOn());
        CHECK_FALSE(bench.opening());
    }

    void testSensorFaults()
    {
        HeatingBench bench;
        bench.start(25.0);

        // Sonde extérieure coupée : secours à 0 °C, la chauffe continue.
        bench.board.sensorResistances[OUTDOOR] = 5.0;
        bench.advance(5);
        CHECK_TRUE(bench.pumpOn());
        CHECK_TRUE(bench.movesDuring(60, true));

        // Sonde de départ en défaut : état sûr, la vanne se ferme et la
        // pompe tourne (protection contre le gel).
        bench.shortSensor(FLOW);
        bench.advance(2);
        CHECK_TRUE(bench.closing());
        CHECK_FALSE(bench.opening());
        CHECK_TRUE(bench.pumpOn());
    }
}

void runHeatingCircuitTests()
{
    TestHarness::run("Chauffage démarrage et régulation", testStartupAndRegulation);
    TestHarness::run("Chauffage programme confort / réduit", testSchedule);
    TestHarness::run("Chauffage consigne d'accueil", testHomeSetpoint);
    TestHarness::run("Chauffage arrêt été et post-circulation", testSummerAndOverrun);
    TestHarness::run("Chauffage défauts de sonde", testSensorFaults);
}
