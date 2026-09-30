#ifndef OPC_H
#define OPC_H

#include <Arduino.h>

#include "SensorBoard.h"
#include "Measurement.h"

#include <Adafruit_ST7789.h>
#include <Adafruit_BME280.h>

class OPC
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

    //-------------------------
    // Runtime
    //-------------------------

    void menuPoll();

    bool isIdle() const;

    bool newMeasurement();

    void handlePause();

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

    double adcTemperature;

private:

    void printScreen(
        int16_t x,
        int16_t y,
        uint8_t size,
        uint16_t color,
        const char* text);
};

#endif