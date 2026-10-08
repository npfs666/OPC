// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KILN_INSTALLATION_H
#define KILN_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Temperature/TemperatureTC.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/PWMOutput.h>
#include <Outputs/RelayOutput.h>
#include <Outputs/TimeProportionalActuator.h>

#include <Regulator/LimitAlarm.h>
#include <Regulator/LogicCommand.h>
#include <Regulator/LoopBreakAlarm.h>
#include <Regulator/PID.h>
#include <Regulator/SetpointProgram.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Four électrique de céramique (biscuit, émail), à programme en paliers et
 * rampes.
 *
 * - Sonde 1 : thermocouple K dans le four (type réglable au menu Input :
 *   S, R ou N pour le grès et la porcelaine) ;
 * - Sortie DC 1 : relais statique des résistances, en proportionnel
 *   temporisé (période 10 s) ;
 * - Relais 1 : bobine du contacteur de sécurité, en amont du relais
 *   statique.
 *
 * Trois programmes de 8 segments (cible, vitesse, palier) ; le PID suit la
 * consigne du programme lancé. Démarrer, Arrêter et Segment suivant sont
 * dans le menu Cuisson. Les programmes fournis sont des exemples, à adapter
 * à la terre et aux émaux.
 *
 * La glue ferme le contacteur pendant la cuisson seulement, et l'ouvre :
 * - sur surchauffe (alarme mémorisée, 1300 °C) : la cuisson s'arrête, et ne
 *   redémarre pas avant l'acquittement ;
 * - sur écart haut (four 50 °C au-dessus de la consigne pendant 2 min, hors
 *   rampe descendante ; alarme mémorisée) : relais statique collé, le
 *   programme continue sans chauffe jusqu'à l'acquittement ;
 * - sur défaut du thermocouple ou du PID (état sûr).
 *
 * Le programme ne reprend pas après une coupure de courant. Ces sécurités
 * ne remplacent pas le limiteur de température matériel du four.
 */
class KilnInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void processLogic(uint32_t now) override;

    bool addMenuActions(
        MenuBuilder& menu) const override;

    bool executeMenuAction(
        MenuBuilder::ActionId actionId) override;

    void onMenuActionSaveFailed(
        MenuBuilder::ActionId actionId) override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

    static constexpr MenuBuilder::ActionId START_ACTION = 1;
    static constexpr MenuBuilder::ActionId STOP_ACTION = 2;
    static constexpr MenuBuilder::ActionId SKIP_ACTION = 3;

private:
    // Entrée physique et mesure
    Sensor kilnInput;
    TemperatureTC temperature;

    // Programme et régulation
    SetpointProgram program;
    PID pid;

    // Résistances : relais statique
    TimeProportionalActuator heater;
    PWMOutput heaterOutput;

    // Contacteur de sécurité, piloté par la glue
    LogicCommand contactorCommand;
    ActuatorOnOff contactor;
    RelayOutput contactorRelay;

    // Alarmes
    LimitAlarm overTemperature;
    LimitAlarm deviation;
    LoopBreakAlarm loopAlarm;

    // Journal (cœur contrôle, depuis la glue).
    ProcessControl* process = nullptr;
    SetpointProgram::State previousState = SetpointProgram::State::Idle;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    struct HomeState
    {
        SetpointProgram::State state = SetpointProgram::State::Idle;
        uint8_t program = 0;
        uint8_t selected = 1;
        uint8_t segment = 0;
        uint8_t segmentCount = 0;
        bool setpointValid = false;
        double_t setpoint = 0.0;
        bool targetValid = false;
        double_t target = 0.0;
        bool heldBack = false;
        uint32_t delayRemaining = 0;
        uint32_t soakRemaining = 0;
        uint32_t remaining = 0;
        uint32_t elapsed = 0;
        bool outputValid = false;
        double_t output = 0.0;
    };

    HomeState homeState;
};

#endif
