#ifndef OPC_H
#define OPC_H

#include <Arduino.h>

#include <InterCoreMessages.h>
#include "Hardware/SensorBoard.h"
#include <ProcessControl.h>

#include <Adafruit_ST7789.h>
#include <Adafruit_BME280.h>

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
    void initBME280();
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

    Adafruit_BME280 bme;

    ProcessControl controller;

private:
    static constexpr size_t SERIAL_PRINT_BUFFER_SIZE =
        1024;

    enum class UIState : uint8_t
    {
        Starting,
        Home,
        PauseRequested,
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

    bool acquisitionPausedForMenu = false;
    bool controlOutputsEnabled = false;
    bool bmeInitialized = false;
    bool configurationSavePending = false;
    uint32_t lastMeasurementTime = 0;

    MenuBuilder::ActionId pendingMenuAction =
        MenuBuilder::NO_ACTION;

    mutex_t processDataMutex;
    ProcessSnapshot sharedProcessSnapshot;
    ProcessSnapshot displayProcessSnapshot;
    uint8_t serialPrintBuffer[
        SERIAL_PRINT_BUFFER_SIZE] = {};

    void copyProcessSnapshot();

    void showHomeScreen(
        bool fullRefresh);

    void requestMenu();

    void requestParameterApply(
        MenuBuilder::ActionId actionId =
            MenuBuilder::NO_ACTION);

    bool validateRestoredParameters(
        const ParameterEditor& editor) const override;
};

#endif
