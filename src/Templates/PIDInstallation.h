#ifndef PID_INSTALLATION_H
#define PID_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/RelayOutput.h>
#include <Outputs/TimeProportionalActuator.h>

#include <Regulator/LimitAlarm.h>
#include <Regulator/LoopBreakAlarm.h>
#include <Regulator/PID.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Installation PID simple pour piloter un chauffage ou un refroidissement.
 *
 * Le réglage manuel est disponible dans le menu « PID ». L'autotune est une
 * option séparée : il calcule les gains sans démarrer ensuite la régulation.
 * Le mode choisi doit correspondre à l'équipement raccordé au relais.
 */
class PIDInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void onParametersApplied() override;

    bool addMenuActions(
        MenuBuilder& menu) const override;

    bool executeMenuAction(
        MenuBuilder::ActionId actionId) override;

    void onMenuActionSaveFailed(
        MenuBuilder::ActionId actionId) override;

    bool takeConfigurationSaveRequest() override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    struct HomeState
    {
        PID::Mode mode = PID::Mode::Heating;
        PID::AutoTuneStatus autoTuneStatus =
            PID::AutoTuneStatus::Idle;

        double_t setpoint = 0.0;
        uint8_t completedCycles = 0;
        uint8_t requestedCycles = 0;

        bool pidEnabled = false;
        bool autoTuneActive = false;
        bool fallback = false;              // maintien sur défaut capteur
        bool manual = false;                // sortie fixée par l'opérateur
        bool interlocked = false;           // verrouillé par l'alarme de boucle

        // Commande du PID (0 à 1), invalide si la régulation est arrêtée.
        double_t output = 0.0;
        bool outputValid = false;

        // Consigne active encore en route vers la consigne réglée.
        bool ramping = false;

        double_t kp = 0.0;
        double_t ti = 0.0;
        double_t td = 0.0;
    };

    // Entrée physique
    Sensor temperatureInput;

    // Mesures
    Resistance temperatureResistance;
    TemperatureRTD temperature;

    // Régulation
    PID pid;

    // Commande temporisée du relais
    TimeProportionalActuator actuator;
    RelayOutput controlRelay;

    // Alarmes (affichées seulement, désactivées par défaut)
    LimitAlarm alarm;
    LoopBreakAlarm loopAlarm;

    HomeState homeState;

    static constexpr MenuBuilder::ActionId
        START_AUTOTUNE_ACTION = 1;
};

#endif
