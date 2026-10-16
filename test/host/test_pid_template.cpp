// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TemplateBench.h"

#include <Hardware/pinout.h>
#include <Templates/PIDInstallation.h>
#include <hardware/gpio.h>

/*
 * Template PID complet sur l'hôte : PT100 simulée par sa résistance,
 * relais statique sur la sortie DC 1.
 */
namespace
{
    struct PidBench : TemplateBench<PIDInstallation>
    {
        bool ssrOn() const
        {
            return FakeGPIO::levels[Board::Rp2040::OUTPUT_3];
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

        // Régulation arrêtée par défaut : aucune chauffe.
        CHECK_TRUE(bench.heatingDuring(30) == 0);

        // Activée, consigne 35 °C, mesure à 20 °C : le relais statique
        // chauffe.
        bench.setBool("tune_pid", "enabled", true);
        CHECK_TRUE(bench.heatingDuring(30) > 0);

        // Sonde en court-circuit : sortie coupée.
        bench.shortSensor(0);
        bench.advance(2);
        CHECK_TRUE(bench.heatingDuring(30) == 0);
    }
}
