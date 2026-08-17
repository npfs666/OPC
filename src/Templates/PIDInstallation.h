#ifndef PID_INSTALLATION_H
#define PID_INSTALLATION_H

#include <Installation.h>

#include <Hardware/RTDSensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/RelayOutput.h>
#include <Outputs/TimeProportionalActuator.h>

#include <Regulator/PID.h>

class Adafruit_BME280;
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
        Adafruit_BME280& bme,
        ProcessControl& process) override;

    void onMenuOpened() override;

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
    };

    // Entrée physique
    RTDSensor temperatureInput;

    // Mesures
    Resistance temperatureResistance;
    TemperatureRTD temperature;

    // Régulation
    PID pid;

    // Commande temporisée du relais
    TimeProportionalActuator actuator;
    RelayOutput controlRelay;

    HomeState homeState;

    static constexpr MenuBuilder::ActionId
        START_AUTOTUNE_ACTION = 1;
};

#endif
