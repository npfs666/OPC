#include "OPC.h"

#include <SPI.h>
#include <Wire.h>
#include <Hardware/pinout.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Temperature/TemperatureBME.h>
#include <Measurements/Humidity/HumidityBME.h>
#include <Measurements/Pressure/PressureBME.h>
#include <Measurements/Humidity/HumidityPsychrometer.h>
#include <Regulator/Thermostat.h>

OPC::OPC() : tft(&SPI1, LCD_CS, LCD_DC, LCD_RESET)
{
}

/*OPC::OPC() : tft(&SPI1, LCD_CS, LCD_DC, LCD_RESET), userInstallation(NULL)
{
}*/

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
        Adafruit_BME280::MODE_NORMAL,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::FILTER_OFF,
        Adafruit_BME280::STANDBY_MS_0_5);
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
    encoder.begin();
}

void OPC::handleISRRotenc()
{
    encoder.onRotationISR();
}

void OPC::handleISRButton()
{
    encoder.onButtonISR();
}

bool OPC::newMeasurement()
{
    if(!input.newMeasurement)
        return false;

    input.newMeasurement = false;
    
    // Faire la MAJ des mesures (conversion data -> mesure)
    unsigned long times = millis();
    controller.update(times);

    rp2040.fifo.push_nb(PRINT_DATA_AVAILABLE);

    return true;
}

void OPC::initMeasurements()
{
    userInstall.begin(input, bme, controller);

    input.startContinuous();
}

void OPC::initMenu()
{
    if (menu.isInitialized())
        return;

    parameterEditor.begin(userInstall.getParameters());
    parameterEditor.capture();

    if (!menuDefinition.begin("Parametres"))
    {
        Serial.println("Menu definition initialization failed");
        return;
    }

    userInstall.buildMenu(menuDefinition);

    if (!menu.begin(
            tft,
            parameterEditor,
            menuDefinition))
    {
        Serial.println("Menu initialization failed");
    }
}

void OPC::menuPoll()
{
    int32_t movement = encoder.takeRotation();

    while (movement > 0)
    {
        menu.move(1);
        movement--;
    }

    while (movement < 0)
    {
        menu.move(-1);
        movement++;
    }

    if (encoder.takeClick())
        menu.enter();

    menu.poll();
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
