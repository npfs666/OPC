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
    void initMeasurements();

    //-------------------------
    // Runtime
    //-------------------------

    void menuPoll();

    //bool isIdle() const;

    bool newMeasurement();

    void handleISRPause();

    void handleISRRotenc();

    void handleISRButton();

    //-------------------------
    // Affichage
    //-------------------------

    void displayMeasurements(
        Measurement* measurements,
        uint8_t count);

    void serialMeasurements(
        Measurement* measurements,
        uint8_t count);

    //-------------------------
    // Hardware
    //-------------------------

    SensorBoard input;

    Adafruit_ST7789 tft;

    Adafruit_BME280 bme;

    ProcessControl controller;

    void printScreen(int16_t x, int16_t y, uint8_t size, uint16_t color, const char* text);

private:

    TestInstallation userInstall;
    ParameterEditor parameterEditor;
    MenuBuilder menuDefinition;
    RotaryEncoder encoder;
    ArduinoMenuUI menu;
};

#endif
