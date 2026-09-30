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

    enum class UIState : uint8_t
    {
        Starting,
        StartupFailed,
        Home,
        CaptureRequested,
        ClockCaptureRequested,
        ClockApplyRequested,
        Menu,
        ApplyRequested
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
    uint32_t lastMeasurementTime = 0;

    // Écrit par le cœur contrôle avant StartupFailed, lu ensuite par le cœur UI.
    StartupError startupError = StartupError::None;
    uint32_t lastStartupErrorPrint = 0;

    MenuBuilder::ActionId pendingMenuAction =
        MenuBuilder::NO_ACTION;

    mutex_t processDataMutex;
    ProcessSnapshot sharedProcessSnapshot;
    ProcessSnapshot displayProcessSnapshot;
    RTC::DateTime sharedClockDateTime;
    bool sharedClockValid = false;
    uint32_t lastClockRefresh = 0;
    uint8_t serialPrintBuffer[
        SERIAL_PRINT_BUFFER_SIZE] = {};

    bool failStartup(StartupError error);
    const char* startupErrorDetail() const;
    void printStartupError(Print& output) const;
    void showStartupError();

    void copyProcessSnapshot();

    void showHomeScreen(
        bool fullRefresh);

    void requestMenu();
    void requestClockApply();

    void requestParameterApply(
        MenuBuilder::ActionId actionId =
            MenuBuilder::NO_ACTION);

    bool validateRestoredParameters(
        const ParameterEditor& editor) const override;
};

#endif
