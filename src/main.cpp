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

#include <OPC.h>
#include <testInstallation.h>
#include <hardware/sync.h>


namespace
{
    TestInstallation installation;
    OPC opc(installation);

    void synchronizeStartup(uint32_t localCoreReady,uint32_t remoteCoreReady)
    {
        rp2040.fifo.push(localCoreReady);

        uint32_t message;

        do
        {
            message = rp2040.fifo.pop();
        }
        while (message != remoteCoreReady);

        /*
         * Toutes les initialisations effectuées avant
         * la barrière sont maintenant visibles par
         * les deux coeurs.
         */
        __dmb();
    }

    void adcInterrupt()
    {
        opc.input.adcInterrupt();
    }

    void ISRRotenc()
    {
        opc.handleISRRotenc();
    }

    void ISRButton()
    {
        opc.handleISRButton();
    }
}



/**
 * CPU0 : contrôle de la mesure ADC et de la régulation
 */
void setup()
{
	opc.initSensorBoard();

    synchronizeStartup(
        CONTROL_CORE_READY,
        UI_CORE_READY);

	attachInterrupt(digitalPinToInterrupt(SPI_DRDY), adcInterrupt, FALLING);
    
    if (opc.initMeasurements())
        rp2040.fifo.push(PARAMETERS_READY);
}



/**
 * CPU0
 */
void loop()
{	
    uint32_t message;

    while (rp2040.fifo.pop_nb(&message))
        opc.handleControlMessage(message);

    // Mettre en place le calcul des measurement, car ici on est pas dans l'ISR donc on a le temps.
    opc.newMeasurement();

    opc.controlPoll();
}



/**
 * Cpu1 : contrôle des IT utilisateur et écran
 */
void setup1()
{
	opc.initSerial();
	opc.initRotenc();
    attachInterrupt(digitalPinToInterrupt(ROTENC_A), ISRRotenc, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROTENC_B), ISRRotenc, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROTENC_CLIC), ISRButton, FALLING);
	opc.initDisplay();
	opc.initBME280();

    synchronizeStartup(
        UI_CORE_READY,
        CONTROL_CORE_READY);
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
 * Cpu1
 */
void loop1()
{

    uint32_t message;

    while (rp2040.fifo.pop_nb(&message))
        opc.handleUIMessage(message);

    opc.uiPoll();
}
