#ifndef OPC_H
#define OPC_H

#include <Arduino.h>

#include <InterCoreMessages.h>
#include <StartupStatus.h>
#include "Hardware/SensorBoard.h"
#include <Hardware/RTC.h>
#include <ProcessControl.h>

#include <Adafruit_ST7789.h>
#include <Adafruit_BMP5xx.h>

#include <ProcessSnapshot.h>

#include <hmi/MenuBuilder.h>
#include <hmi/ParameterEditor.h>
#include <Storage.h>
#include <hmi/RotaryEncoder.h>
#include <hmi/ArduinoMenuUI.h>
#include <hmi/HomeSetpointEditor.h>

#include <pico/mutex.h>

class Installation;

class OPC : private ParameterRestoreValidator
{
public:

    explicit OPC(Installation& installation);

    //-------------------------
    // Initialisation
    //-------------------------

    void initSerial();
    void initDisplay();
    void initI2C();
    void initRTC();
    void initBMP580();
    void initSensorBoard();
    void initRotenc();
    void initMenu();
    bool initMeasurements();

    //-------------------------
    // Runtime
    //-------------------------

    void controlPoll();

    void uiPoll();

    void handleControlMessage(
        InterCoreMessage message);

    void handleUIMessage(
        InterCoreMessage message);

    bool newMeasurement();

    void handleISRRotenc();

    void handleISRButton();

    //-------------------------
    // Hardware
    //-------------------------

    SensorBoard input;

    Adafruit_ST7789 tft;

    Adafruit_BMP5xx bmp580;

    ProcessControl controller;

    RTC clock;

private:
    static constexpr size_t SERIAL_PRINT_BUFFER_SIZE =
        1024;

    // Période de rappel de l'erreur de démarrage sur le port série.
    static constexpr uint32_t STARTUP_ERROR_REPEAT_MS =
        5000;

    // Période de régulation d'une installation sans entrée analogique.
    static constexpr uint32_t SENSORLESS_CYCLE_MS =
        1000;

    // Sauvegarde d'une consigne réglée depuis l'accueil : après ce délai sans
    // nouvelle modification, pour ne pas écrire la flash à chaque réglage.
    static constexpr uint32_t HOME_SETPOINT_SAVE_DELAY_MS =
        10000;

    // Lectures du DS3231 ratées tolérées avant de déclarer l'heure inconnue.
    static constexpr uint8_t CLOCK_READ_FAILURES_TOLERATED =
        3;

    enum class UIState : uint8_t
    {
        Starting,
        StartupFailed,
        Home,
        CaptureRequested,
        ClockCaptureRequested,
        ClockApplyRequested,
        Menu,
        ApplyRequested,
        HomeSetpointApplyRequested
    };

    Installation& userInstall;
    Storage storage;
    ParameterEditor parameterEditor;
    MenuBuilder menuDefinition;
    RotaryEncoder encoder;
    ArduinoMenuUI menu;

    UIState uiState = UIState::Starting;
    uint32_t lastMenuActivity = 0;
    bool clockMenuOpen = false;

    bool acquisitionPausedForMenu = false;
    bool menuSessionOpen = false;
    bool controlOutputsEnabled = false;
    bool bmp580Initialized = false;
    bool sensorBoardInitialized = false;
    bool configurationSavePending = false;
    uint32_t configurationSaveDueAt = 0;
    bool controlCycleStarted = false;
    uint32_t lastMeasurementTime = 0;

    // Écrit par le cœur contrôle avant StartupFailed, lu ensuite par le cœur UI.
    StartupError startupError = StartupError::None;
    uint32_t lastStartupErrorPrint = 0;

    MenuBuilder::ActionId pendingMenuAction =
        MenuBuilder::NO_ACTION;

    mutex_t processDataMutex;
    // Consigne réglée depuis l'accueil, transmise sous le mutex.
    double_t pendingHomeSetpoint = 0.0;
    ProcessSnapshot sharedProcessSnapshot;
    ProcessSnapshot displayProcessSnapshot;
    RTC::DateTime sharedClockDateTime;
    bool sharedClockValid = false;
    uint32_t lastClockRefresh = 0;
    uint8_t clockReadFailures = 0;

    // Cœur UI uniquement.
    bool clockAlertShown = false;
    HomeSetpointEditor setpointEditor;
    uint8_t serialPrintBuffer[
        SERIAL_PRINT_BUFFER_SIZE] = {};

    bool failStartup(StartupError error);
    const char* startupErrorDetail() const;
    void printStartupError(Print& output) const;
    void showStartupError();

    void refreshClock();
    void showClockAlert();

    void copyProcessSnapshot();

    void showHomeScreen(
        bool fullRefresh);

    void requestMenu();
    void requestClockApply();

    // Cœur UI : encodeur à l'accueil (réglage de la consigne ou menu).
    void homePoll(int32_t movement, bool clicked);
    void commitHomeSetpoint();

    // Cœur contrôle : sauvegarde après delayMs (repoussée par une nouvelle
    // demande différée, immédiate si delayMs vaut 0).
    void requestConfigurationSave(uint32_t delayMs = 0);

    void requestParameterApply(
        MenuBuilder::ActionId actionId =
            MenuBuilder::NO_ACTION);

    bool validateRestoredParameters(
        const ParameterEditor& editor) const override;
};

#endif
