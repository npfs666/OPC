// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TemplateBench.h"

#include <Hardware/pinout.h>
#include <Templates/PIDInstallation.h>
#include <hardware/gpio.h>

/*
 * Template PID complet sur l'hôte : PT100 simulée par sa résistance,
 * relais statique sur la sortie DC 1, contacteur général sur le relais 1.
 */
namespace
{
    struct PidBench : TemplateBench<PIDInstallation>
    {
        bool ssrOn() const
        {
            return FakeGPIO::levels[Board::Rp2040::OUTPUT_3];
        }

        bool contactorClosed() const
        {
            return FakeDigitalIO::levels[Board::Rp2040::OUTPUT_1] == HIGH;
        }

        // Secondes de chauffe du relais statique pendant la durée donnée.
        uint32_t heatingDuring(uint32_t seconds)
        {
            uint32_t on = 0;

            for (uint32_t i = 0; i < seconds; i++)
            {
                cycle();

                if (ssrOn())
                    on++;
            }

            return on;
        }
    };

    TEST_CASE("Template PID : relais statique sur la sortie DC 1")
    {
        PidBench bench;
        CHECK_TRUE(bench.started);

        // Sortie PWM verrouillée à l'arrêt en sécurité ; plus de relais
        // mécanique.
        CHECK_TRUE(bench.parameter("pid_ssr", "pin") != nullptr);
        CHECK_TRUE(bench.parameter("pid_tune_relay", "pin") == nullptr);

        bench.setTemperature(0, 20.0);
        bench.advance(2);

        // Régulation arrêtée par défaut : aucune chauffe, contacteur ouvert.
        CHECK_TRUE(bench.heatingDuring(30) == 0);
        CHECK_FALSE(bench.contactorClosed());

        // Activée, consigne 35 °C, mesure à 20 °C : le contacteur se ferme
        // et le relais statique chauffe.
        bench.setBool("tune_pid", "enabled", true);
        CHECK_TRUE(bench.heatingDuring(30) > 0);
        CHECK_TRUE(bench.contactorClosed());

        // Sonde en court-circuit : sortie coupée, contacteur ouvert (un SSR
        // en court-circuit serait coupé).
        bench.shortSensor(0);
        bench.advance(2);
        CHECK_TRUE(bench.heatingDuring(30) == 0);
        CHECK_FALSE(bench.contactorClosed());

        // Retour de la sonde : la régulation reprend.
        bench.setTemperature(0, 20.0);
        bench.advance(2);
        CHECK_TRUE(bench.contactorClosed());

        // Arrêt de la régulation : contacteur ouvert.
        bench.setBool("tune_pid", "enabled", false);
        bench.advance(2);
        CHECK_FALSE(bench.contactorClosed());
    }

    TEST_CASE("Template PID : contacteur en mode manuel")
    {
        // Manuel à 50 %, régulation non activée : le contacteur se ferme,
        // même sonde en défaut (le manuel garde la main).
        PidBench bench;
        bench.setTemperature(0, 20.0);
        bench.advance(2);
        bench.setDiscrete(
            "tune_pid", "operation",
            static_cast<int32_t>(PID::Operation::Manual));
        bench.setNumber("tune_pid", "manual_output", 50.0);
        bench.advance(2);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) > 0);

        bench.shortSensor(0);
        bench.advance(2);
        CHECK_TRUE(bench.contactorClosed());
        CHECK_TRUE(bench.heatingDuring(30) > 0);
    }
}
