/**
 * @file main.cpp
 *
 * @author GAOU (arstaligtredan.fr)
 * @version 0.3
 * @date 2026-07-12
 *
 * @copyright Copyright (c) 2022
 * 
 * MIT license, all text above must be included in any redistribution 
 */
//#include <Hardware/pinout.h>
#include <OPC.h>
//#include "pico/stdlib.h"
//#include <math.h>
//#include <EEPROM.h>
//#include <Measurements/Resistance.h>
//#include <Measurements/Temperature/TemperatureRTD.h>



namespace
{
    OPC opc;
}

void adcInterrupt();
void ISRRotenc();
void ISRButton();

/**
 * CPU0 : contrôle de la mesure ADC et de la régulation
 */
void setup()
{
	delay(1200);

	opc.initSensorBoard();

	attachInterrupt(digitalPinToInterrupt(SPI_DRDY), adcInterrupt, FALLING);
    
    opc.initMeasurements();

    rp2040.fifo.push(PARAMETERS_READY);
}



void adcInterrupt() {

	opc.input.adcInterrupt();
}

void ISRRotenc() {
    //Serial.println("rotenc");
    opc.handleISRRotenc();
}

void ISRButton() {
    //Serial.println("clic");
    opc.handleISRButton();
}


/**
 * CPU0 : contrôle de la mesure ADC et de la régulation
 */
void loop()
{	
    // opc.handleISRPause();
    // Gestion des messages entre CPU0 et CPU1 pour la mise en pause des ISR en cas d'entrée dans le menu
	if (rp2040.fifo.available() > 0) {
		uint32_t val = rp2040.fifo.pop();
		
		// Pauses ADC IRQ while calibrating
		if (val == PAUSE_ADC_INTERRUPTS) {
			irq_set_enabled(13, false); // Pause IO interrupts
		} else if (val == RESUME_ADC_INTERRUPTS) {
			irq_set_enabled(13, true); // Resume IO interrupts
		}
	}

    // Mettre en place le calcul des measurement, car ici on est pas dans l'ISR donc on a le temps.
    if (!opc.newMeasurement())  return;
}



/**
 * Cpu1 : contrôle des IT utilisateur et écran
 */
void setup1()
{
    delay(100);
	opc.initSerial();
	opc.initRotenc();
    attachInterrupt(digitalPinToInterrupt(ROTENC_A),ISRRotenc,FALLING);
    attachInterrupt(digitalPinToInterrupt(ROTENC_CLIC), ISRButton, FALLING);
	opc.initDisplay();
	opc.initBME280();
}


/**
 * ATTENTION ! Avec ce système de refresh timé, il faut que le temps de mesure soit plus court !
 * Système uniquement valable pour des mesures d'essai
 */
/*uint32_t currentTime, previousTime = 3000, refreshTime = 7000;     // All in ms
 currentTime = millis();

    // Refresh loop, every refreshTime ms
    if ( (currentTime - previousTime) >= refreshTime) {
        opc.controller.printCSVPsychro(currentTime);

        opc.input.restart(); // Once UI update is done we can restart conversions.
        
        previousTime = currentTime;  // Remember the time
    }
*/
/**
 * Cpu1 : contrôle des IT utilisateur et écran
 */
void loop1()
{

    uint32_t message;

    while (rp2040.fifo.pop_nb(&message))
    {
        switch (message)
        {
        case PARAMETERS_READY:
            opc.initMenu();
            break;

        case PRINT_DATA_AVAILABLE:
            opc.controller.print(Serial);
            break;
        }
    }

    opc.menuPoll();

    // Affichage écran
    //char TX[50];

    // Affichage de la mesure
    /*double_t res1, res2;

    //res1 = opc.input.rtd[0].readValue();
    res1 = opc.controller.getMeasurement(3)->getValue();
    res2 = opc.controller.getMeasurement(5)->getValue();
    
    opc.printScreen(0, 5, 4, ST77XX_ORANGE, "test");
    sprintf(TX, "%.2lf", res1);
    opc.printScreen(0, 40, 4, ST77XX_ORANGE, TX);
    sprintf(TX, "%.2lf", res2);
    opc.printScreen(0, 80, 4, ST77XX_ORANGE, TX);*/
}






