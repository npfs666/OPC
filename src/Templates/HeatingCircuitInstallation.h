// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HEATING_CIRCUIT_INSTALLATION_H
#define HEATING_CIRCUIT_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/PWMOutput.h>
#include <Outputs/RelayOutput.h>
#include <Outputs/ThreePointActuator.h>

#include <Regulator/DelayTimer.h>
#include <Regulator/HeatingCurve.h>
#include <Regulator/LimitAlarm.h>
#include <Regulator/PID.h>
#include <Regulator/TimeSchedule.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Circuit de chauffage hydraulique à vanne mélangeuse, régulé par loi d'eau.
 *
 * - Sonde 1 : départ du circuit (Pt100), après la vanne et la pompe ;
 * - Sonde 2 : extérieure (Pt1000), au nord, à l'abri du soleil ;
 * - Relais 1 et 2 : vanne 3 points, Ouvrir et Fermer. En défaut, la vanne
 *   se ferme (état sûr du relais Fermer, réglable) ;
 * - Sortie DC 1 : pompe, par un relais d'interface (sortie à collecteur
 *   ouvert). En défaut, la pompe tourne (protection contre le gel,
 *   réglable).
 *
 * La loi d'eau calcule le départ d'après l'extérieur et la consigne
 * d'ambiance (confort pendant le programme, réduite en dehors), et demande
 * la chauffe. Le PID du départ suit cette consigne et pilote la vanne. La
 * pompe suit la demande de chauffe, avec post-circulation. Arrêt été,
 * hors-gel et secours sur défaut de la sonde extérieure : voir HeatingCurve.
 *
 * Sans glue : tout est relié dans begin(). L'alarme de départ haut est à
 * activer au menu pour un plancher chauffant ; elle ne remplace pas le
 * thermostat de sécurité matériel qui coupe la pompe.
 */
class HeatingCircuitInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    // Entrées physiques
    Sensor flowInput;
    Sensor outdoorInput;

    // Mesures
    Resistance flowResistance;
    Resistance outdoorResistance;

    TemperatureRTD flowTemperature;
    TemperatureRTD outdoorTemperature;

    // Loi d'eau et programme confort / réduit
    TimeSchedule comfortSchedule;
    HeatingCurve curve;

    // Départ : PID et vanne 3 points
    PID flowControl;
    ThreePointActuator valve;
    RelayOutput valveOpen;
    RelayOutput valveClose;

    // Pompe : demande de chauffe et post-circulation
    DelayTimer pumpOverrun;
    ActuatorOnOff pump;
    PWMOutput pumpOutput;

    // Alarme de départ haut (plancher), désactivée par défaut
    LimitAlarm flowAlarm;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    struct HomeState
    {
        HeatingCurve::State state = HeatingCurve::State::Waiting;
        double_t outdoor = 0.0;
        bool outdoorFallback = false;
        double_t roomSetpoint = 0.0;
        bool flowSetpointValid = false;
        double_t flowSetpoint = 0.0;
        double_t valvePosition = 0.0;
        bool valveKnown = false;
    };

    HomeState homeState;
};

#endif
