#ifndef OPC_H
#define OPC_H

#include <Arduino.h>

#include "Hardware/SensorBoard.h"
#include <ProcessControl.h>

#include <Adafruit_ST7789.h>
#include <Adafruit_BME280.h>

#include <testInstallation.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Temperature/TemperatureBME.h>
#include <Measurements/Humidity/HumidityBME.h>
#include <Measurements/Pressure/PressureBME.h>
#include <Measurements/Humidity/HumidityPsychrometer.h>

#include <MenuBuilder.h>
#include <ParameterEditor.h>
#include "hmi/RotaryEncoder.h"
#include <hmi/ArduinoMenuUI.h>
#include <hmi/HomeScreen.h>

#include <pico/mutex.h>

class OPC
{
public:

    #define MEASURE_SIZE 10

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
    ParameterEditor parameterEditor;
    MenuBuilder menuDefinition;
    RotaryEncoder encoder;
    ArduinoMenuUI menu;

    UIState uiState = UIState::Starting;
    uint32_t lastMenuActivity = 0;

    bool acquisitionPausedForMenu = false;

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
};

#endif
