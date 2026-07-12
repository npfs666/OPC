/**
 * @file main.cpp
 *
 *
 *
 * @author GAOU (arstaligtredan.fr)
 * @brief
 * @version 0.1
 * @date 2022-02-07
 *
 * @copyright Copyright (c) 2022
 *
 * MIT license, all text above must be included in any redistribution 
 */
#include <Hardware/pinout.h>
#include <OPC.h>
#include "pico/stdlib.h"
#include <math.h>
#include <EEPROM.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>


namespace
{
    OPC opc;
}

void adcInterrupt();






void setup()
{
	delay(100);

	opc.initSensorBoard();

	attachInterrupt(digitalPinToInterrupt(SPI_DRDY), adcInterrupt, FALLING);
    

    //opc.input.addRTD(RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::Wire4, 16, 0);
    //opc.input.addRTD(RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::Wire4, 16, 0);
    

    //Resistance r1("R1", opc.input, opc.input.rtd[0]);
    //Resistance r2("R2", input, input.rtd[1]);

    //Temperature t1("T1", r1);
    //Temperature t2("T2", r2);

    //opc.measurements.add(r1);
    //measurements.add(r2);

    //measurements.add(t1);
    //measurements.add(t2);

    //opc.input.startContinuous();
    opc.initMeasurements();
}

/**
 * CPU0 : contrôle de la mesure ADC et de la régulation
 * @brief
 *
 */
void loop()
{	
    // opc.handleISRPause();
    // Gestion des messages entre CPU0 et CPU1 pour la mise en pause des ISR en cas d'entrée dans le menu
	if(  rp2040.fifo.available() > 0) {
		uint32_t val = rp2040.fifo.pop();
		
		// Pauses ADC IRQ while calibrating
		if( val == PAUSE_ADC_INTERRUPTS ) {
			irq_set_enabled(13, false); // Pause IO interrupts
		} else if( val == RESUME_ADC_INTERRUPTS ) {
			irq_set_enabled(13, true); // Resume IO interrupts
		}
	}

    // Mettre en place le calcul des measurement, car ici on est pas dans l'ISR donc on a le temps.
}



void setup1()
{
	opc.initSerial();
	//opc.initRotenc();
	opc.initDisplay();
	opc.initBME280();
}



/**
 * Cpu1 : contrôle des IT utilisateur et écran
 * @brief
 *
 */
void loop1()
{

    if (!opc.newMeasurement())
        return;

    // Affichage écran
    char TX[50];

    // Affichage de la mesure
    double_t res1, res2;

    //res1 = opc.input.rtd[0].readValue();
    res1 = opc.measurements[3].value();
    res2 = opc.measurements[5].value();
    
    opc.printScreen(0, 5, 4, ST77XX_ORANGE, "test");
    sprintf(TX, "%.2lf", res1);
    opc.printScreen(0, 40, 4, ST77XX_ORANGE, TX);
    sprintf(TX, "%.2lf", res2);
    opc.printScreen(0, 80, 4, ST77XX_ORANGE, TX);

    Serial.print("BME = ");Serial.print(opc.measurements[0].value());Serial.println(opc.measurements[0].unit());
    Serial.print("BME = ");Serial.print(opc.measurements[1].value());Serial.println(opc.measurements[1].unit());
    Serial.print("r1 = ");Serial.println(res1, 3);
    Serial.print("r2 = ");Serial.println(res2, 3);

    //Serial.print("RH = ");Serial.println(Psychrometer::relativeHumidity(25,20,98025), 3);
    //Serial.print("RH = ");Serial.println(Psychrometer::relativeHumidity(25,20,104025), 3);
    //Serial.print("SVP = ");Serial.println(Psychrometer::saturationVaporPressure(25), 3);
    //Serial.print("VP = ");Serial.println(Psychrometer::saturationVaporPressure(20), 3);

    //Serial.print("RH2 = ");Serial.println(Psychrometer::getRH(25,20,98025), 3);
    //Serial.print("RH2 = ");Serial.println(Psychrometer::getRH(25,20,104025), 3);

    //delay(1000);
}













void adcInterrupt() {

	opc.input.adcInterrupt();
}