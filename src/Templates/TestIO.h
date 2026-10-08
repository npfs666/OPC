// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Installation.h>
#include <Hardware/Sensor.h>
#include <Inputs/DigitalInput.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Outputs/ActuatorOnOff.h>
#include <Outputs/ActuatorPWM.h>
#include <Outputs/PWMOutput.h>
#include <Outputs/RelayOutput.h>
#include <Regulator/LogicCommand.h>

/*
 * Test matériel : entrée N -> relais N et PWM N à 50 %, 20 kHz.
 *
 * Banc d'essai de la glue (processLogic()), avec des fonctions fictives :
 * - la commande 2 dépend aussi de la PT100 : sonde débranchée, relais 2 et
 *   PWM 2 en état sûr ;
 * - la commande 1 garde son réglage « Commande » (Auto / Marche / Arrêt),
 *   la commande 2 n'a pas de mode manuel ;
 * - l'action « Simuler oubli glue » cesse d'écrire la commande 1 pendant
 *   quelques secondes : état sûr et événement dans le journal.
 */
class TestIO final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void processLogic(uint32_t now) override;

    void captureHomeScreenState() override;

    void printHomeScreen(HomeScreenContext& context) override;

    bool addMenuActions(MenuBuilder& menu) const override;

    bool executeMenuAction(MenuBuilder::ActionId actionId) override;

    void onMenuActionSaveFailed(MenuBuilder::ActionId actionId) override;

private:
    static constexpr MenuBuilder::ActionId
        SIMULATE_MISSING_WRITE_ACTION = 1;

    static constexpr uint32_t MISSING_WRITE_DURATION_MS = 5000;

    Sensor pt100Input;
    Resistance pt100Resistance;
    TemperatureRTD pt100Temperature;

    DigitalInput inputs[2];
    LogicCommand commands[2];
    ActuatorOnOff relayActuators[2];
    ActuatorPWM pwmActuators[2];
    RelayOutput relays[2];
    PWMOutput pwms[2];

    // Oubli d'écriture simulé de la commande 1.
    bool missingWrite = false;
    uint32_t missingWriteStart = 0;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    LogicCommand::Operation commandOneOperation =
        LogicCommand::Operation::Auto;
};
