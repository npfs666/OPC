#include "Hardware/pinout.h"
#include "OPC.h"

#include <SPI.h>
#include <Wire.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Temperature/TemperatureBME.h>
#include <Measurements/Humidity/HumidityBME.h>
#include <Measurements/Pressure/PressureBME.h>

OPC::OPC() : tft(&SPI1, LCD_CS, LCD_DC, LCD_RESET)
{
}

void OPC::initSerial()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println("Open Process Controller v0.3");
}

void OPC::initDisplay()
{
    SPI1.setSCK(LCD_SCK);
    SPI1.setTX(LCD_MOSI);

    tft.init(135, 240);
    tft.setRotation(3);
    tft.setSPISpeed(48000000);
    tft.setTextWrap(false);
    tft.cp437(true);
    tft.fillScreen(ST77XX_BLACK);
}

void OPC::initBME280()
{
    Wire.setSDA(BME_SDA);
    Wire.setSCL(BME_SCL);

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
    // Set pin 23 HIGH to switch the pico DC-DC converter to PWM (improved ripple)
	// Improves a lot measurement stability
    pinMode(23,OUTPUT);
    digitalWrite(23,HIGH);

    input.init();
}


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
  
    // MAJ du BME280
    bme.takeForcedMeasurement();    

    // Faire la MAJ des mesures (conversion data -> mesure)
    measurements.update();

    return true;
}

void OPC::initMeasurements() {

    input.addRTD(RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 16, 0);
    input.addRTD(RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 16, 0);
    
    auto* tempBME = new TemperatureBME("BME", bme);
    measurements.add(*tempBME);

    auto* rhBME = new HumidityBME("BME", bme);
    measurements.add(*rhBME);

    auto* paBME = new PressureBME("BME",bme);
    measurements.add(*paBME);

    auto* r = new Resistance("rRTD 1", input, input.rtd[0]);
    auto* t = new TemperatureRTD("TempRTD 1", *r);
    measurements.add(*r);
    measurements.add(*t);

    auto* r1 = new Resistance("rRTD 2", input, input.rtd[1]);
    auto* t1 = new TemperatureRTD("TempRTD 2", *r1);
    measurements.add(*r1);
    measurements.add(*t1);

    input.startContinuous();
}

void OPC::menuPoll()
{
    //nav.poll();
}

void OPC::handleISRPause()
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

void OPC::displayMeasurements(Measurement* measurements, uint8_t count)
{
    Serial.println("Display measurements:");
    for(uint8_t i = 0; i < count; i++)
    {

        /*Serial.print(measurements[i].name);
        Serial.print(" = ");
        Serial.print(measurements[i].value, 2);
        Serial.print(" ");
        Serial.println(measurements[i].unit);*/
    }
}

void OPC::serialMeasurements(Measurement* measurements, uint8_t count)
{
    Serial.println("Serial measurements:");
    for(uint8_t i = 0; i < count; i++)
    {
        /*Serial.print(measurements[i].name);
        Serial.print(": ");
        Serial.print(measurements[i].value, 2);
        Serial.print(" ");
        Serial.println(measurements[i].unit);*/
    }
    Serial.println();
}

void OPC::printScreen(int16_t x, int16_t y, uint8_t size, uint16_t color,const char* text) {

    tft.setTextSize(4);
    tft.setCursor(x, y);
    tft.setTextColor(color, ST77XX_BLACK);
    tft.printf(text);
}