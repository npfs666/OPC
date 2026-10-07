#ifndef COLD_ROOM_INSTALLATION_H
#define COLD_ROOM_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>
#include <Inputs/DigitalInput.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/PWMOutput.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/Comparator.h>
#include <Regulator/ConditionAlarm.h>
#include <Regulator/DelayTimer.h>
#include <Regulator/LimitAlarm.h>
#include <Regulator/LogicCommand.h>
#include <Regulator/Thermostat.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Exemple d'installation : chambre froide positive (fromagerie, 2 à 4 °C).
 *
 * - Sonde 1 : ambiance ; sonde 2 : évaporateur ; entrée TOR 1 : porte.
 * - Relais 1 : compresseur, thermostat froid sur l'ambiance ;
 * - Relais 2 : ventilateurs de l'évaporateur ;
 * - PWM 1 : résistance de dégivrage, par un relais statique.
 *
 * La glue (processLogic()) enchaîne le cycle Froid -> Dégivrage (toutes les
 * 6 h) -> Égouttage -> Reprise : elle inhibe le thermostat pendant le
 * dégivrage et l'égouttage, pilote la résistance et les ventilateurs, et
 * masque l'alarme de température haute pendant le cycle.
 *
 * Ce template n'est pas sélectionné par OPC. Il peut être copié,
 * renommé et adapté avant de remplacer l'installation utilisateur.
 */
class ColdRoomInstallation final : public Installation
{
public:
    /** Étape du cycle de dégivrage. */
    enum class Phase : uint8_t
    {
        Cooling,    // froid : thermostat et ventilateurs
        Defrost,    // résistance, compresseur et ventilateurs arrêtés
        Drip,       // égouttage : tout arrêté
        Recovery    // reprise : compresseur seul, ventilateurs retardés
    };

    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void processLogic(uint32_t now) override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    // Entrées physiques
    Sensor ambientInput;
    Sensor evaporatorInput;
    DigitalInput door;

    // Mesures
    Resistance ambientResistance;
    Resistance evaporatorResistance;
    TemperatureRTD ambientTemperature;
    TemperatureRTD evaporatorTemperature;

    // Froid : thermostat sur l'ambiance, relais du compresseur
    Thermostat thermostat;
    ActuatorOnOff compressor;
    RelayOutput compressorRelay;

    // Dégivrage
    bool defrostEnabled = true;
    Comparator defrostEnd;          // évaporateur assez chaud
    DelayTimer defrostInterval;     // temps de froid avant un dégivrage
    DelayTimer defrostMaximum;      // durée max d'un dégivrage
    DelayTimer drip;                // égouttage
    DelayTimer fanDelay;            // retard des ventilateurs à la reprise
    DelayTimer alarmMask;           // masquage de l'alarme après le cycle

    // Sorties écrites par la glue
    LogicCommand fanCommand;
    ActuatorOnOff fans;
    RelayOutput fanRelay;

    LogicCommand heaterCommand;
    ActuatorOnOff heater;
    PWMOutput heaterOutput;

    // Alarmes (désactivées par défaut)
    ConditionAlarm doorAlarm;
    LimitAlarm highAlarm;

    Phase phase = Phase::Cooling;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    struct HomeState
    {
        Phase phase = Phase::Cooling;
        bool defrostEnabled = true;
        bool manual = false;
        bool doorOpen = false;
        double_t setpoint = NAN;
        uint32_t remainingMs = 0;   // avant la fin de l'étape en cours
    };

    HomeState homeState;
};

#endif
