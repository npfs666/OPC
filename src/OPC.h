#ifndef OPC_H
#define OPC_H

#include <Arduino.h>

#include "Hardware/SensorBoard.h"
#include <ProcessControl.h>

#include <Adafruit_ST7789.h>
#include <Adafruit_BME280.h>

#include <testInstallation.h>
#include <Templates/SolarInstallation.h>
#include <Templates/ThermostatInstallation.h>

#include <Measurements/MeasurementSnapshot.h>

#include <hmi/MenuBuilder.h>
#include <hmi/ParameterEditor.h>
#include <Storage.h>
#include <hmi/RotaryEncoder.h>
#include <hmi/ArduinoMenuUI.h>

#include <pico/mutex.h>

class OPC : private ParameterRestoreValidator
{
public:

    OPC();

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
        uint32_t message);

    void handleUIMessage(
        uint32_t message);

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

    TestInstallation userInstall;
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
    uint32_t lastMeasurementTime = 0;

    mutex_t processDataMutex;
    MeasurementSnapshot sharedMeasurementSnapshot;
    MeasurementSnapshot displayMeasurementSnapshot;
    uint8_t serialPrintBuffer[
        SERIAL_PRINT_BUFFER_SIZE] = {};

    void copyMeasurementSnapshot();

    void showHomeScreen(
        bool fullRefresh);

    void requestMenu();

    void requestParameterApply();

    bool validateRestoredParameters(
        const ParameterEditor& editor) const override;
};

#endif
