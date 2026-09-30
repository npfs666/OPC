#include "Hardware/pinout.h"
#include "OPC.h"

#include <SPI.h>
#include <Wire.h>

OPC::OPC() :
    tft(&SPI1, LCD_CS, LCD_DC, LCD_RESET)
{
    adcTemperature = 0;
}

void OPC::initSerial()
{
    Serial.begin(115200);

    delay(100);

    Serial.println("Open Process Controller");
}

void OPC::initDisplay()
{
    SPI1.setSCK(LCD_SCK);
    SPI1.setTX(LCD_MOSI);

    tft.init(135,240);

    tft.setRotation(3);

    tft.setSPISpeed(48000000);

    tft.setTextWrap(false);

    tft.cp437(true);
}

void OPC::initBME280()
{
    Wire.setSDA(8);
    Wire.setSCL(9);

    bme.begin(0x76,&Wire);

    bme.setSampling(
        Adafruit_BME280::MODE_FORCED,
        Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::FILTER_OFF,
        Adafruit_BME280::STANDBY_MS_1000);
}

void OPC::initSensorBoard()
{
    pinMode(23,OUTPUT);

    digitalWrite(23,HIGH);

    input.init();



void OPC::initRotenc()
{
    pinMode(ROTENC_A,INPUT);

    pinMode(ROTENC_B,INPUT);

    pinMode(ROTENC_CLIC,INPUT);

    /*attachInterrupt(
        digitalPinToInterrupt(ROTENC_A),
        IsrRotenc,
        FALLING);

    attachInterrupt(
        digitalPinToInterrupt(ROTENC_CLIC),
        IsrButton,
        FALLING);*/
}

bool OPC::newMeasurement()
{
    if(!input.newMeasurement)
        return false;

    input.newMeasurement = false;

    return true;
}

void OPC::menuPoll()
{
    //nav.poll();
}

void OPC::handlePause()
{
    if(!rp2040.fifo.available())
        return;

    switch(rp2040.fifo.pop())
    {
        case PAUSE_ADC_INTERRUPTS:

            irq_set_enabled(13,false);

            break;

        case RESUME_ADC_INTERRUPTS:

            irq_set_enabled(13,true);

            //nav.exit();

            break;
    }
}

void OPC::displayMeasurements(
    Measurement* measurements,
    uint8_t count)
{
    Serial.println("Display measurements:");
    for(uint8_t i = 0; i < count; i++)
    {
        if(!measurements[i].enabled)
            continue;

        Serial.print(measurements[i].name);
        Serial.print(" = ");
        Serial.print(measurements[i].value, 2);
        Serial.print(" ");
        Serial.println(measurements[i].unit);
    }
}

void OPC::serialMeasurements(
    Measurement* measurements,
    uint8_t count)
{
    Serial.println("Serial measurements:");
    for(uint8_t i = 0; i < count; i++)
    {
        if(!measurements[i].enabled)
            continue;

        Serial.print(measurements[i].name);
        Serial.print(": ");
        Serial.print(measurements[i].value, 2);
        Serial.print(" ");
        Serial.println(measurements[i].unit);
    }
    Serial.println();
}