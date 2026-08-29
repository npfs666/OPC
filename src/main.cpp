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
#include <SystemWatchdog.h>
#include <testInstallation.h>
#include <Templates/ThermostatInstallation.h>
#include <Templates/SolarInstallation.h>
#include <Templates/PIDInstallation.h>
#include <hardware/sync.h>


namespace
{
    TestInstallation installation;
    OPC opc(installation);
    SystemWatchdog systemWatchdog;

    void synchronizeStartup(
        InterCoreMessage localCoreReady,
        InterCoreMessage remoteCoreReady)
    {
        rp2040.fifo.push(
            interCoreMessageValue(
                localCoreReady));

        uint32_t message;

        do
        {
            message = rp2040.fifo.pop();
        }
        while (message !=
            interCoreMessageValue(
                remoteCoreReady));

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
        Serial.println("AB");
    }

    void ISRButton()
    {
        opc.handleISRButton();
        Serial.println("clic");
    }
}

RTDSensor input1, input2;

/**
 * CPU0 : contrôle de la mesure ADC et de la régulation
 */
void setup()
{
    delay(3000);
    
    opc.initSerial();
	opc.initSensorBoard();
    //opc.initDisplay();
    //opc.initRotenc();

	attachInterrupt(digitalPinToInterrupt(Board::Rp2040::ADC_DRDY), adcInterrupt, FALLING);
    //attachInterrupt(digitalPinToInterrupt(Board::Rp2040::ROTENC_A), ISRRotenc, CHANGE);
    //attachInterrupt(digitalPinToInterrupt(Board::Rp2040::ROTENC_B), ISRRotenc, CHANGE);
    //attachInterrupt(digitalPinToInterrupt(Board::Rp2040::ROTENC_CLIC), ISRButton, FALLING);


    input1.begin("input1", "Input 1", RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 32, 0);
    input2.begin("input2", "Input 2", RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 32, 0);

    //opc.input.executeMenuAction(35); //cal 1650.404 ohm at 25.93 C

    opc.input.addRTD(input1);
    opc.input.addRTD(input2);
    opc.input.startContinuous();

    
    //1650.356 ohm at 24.89 C
}


#include <Physics/PT100.h>
/**
 * CPU0
 */
void loop()
{	

    /*for( int i = 0; i < 3; i++) {
        opc.input.mux.enableChannel(i);
        Serial.println(i+1);
        delay(3000);
        opc.input.mux.disableChannel(0);
        opc.input.mux.disableChannel(1);
        opc.input.mux.disableChannel(2);
    }*/

    if( !opc.input.newMeasurement)
        return;

    opc.input.newMeasurement = false;
    #include <Hardware/RTC.h>
    RTC::DateTime time;
    opc.clock.readDateTime(time);
    char str[50];
    sprintf(str, "%02d:%02d;%02d", time.hour, time.minute, time.second);

    Serial.println("-------------------------");
    Serial.println(str);

    double_t res = opc.input.computeResistance(*opc.input.rtd[0]);
    double_t temp = PT100::getResistanceToTemperature(res);
    Serial.print(opc.input.rtd[0]->readValue());Serial.print("  -  ");
    Serial.print(res,3);Serial.print("  -  ");
    Serial.println(temp,3);

    res = opc.input.computeResistance(*opc.input.rtd[1]);
    temp = PT100::getResistanceToTemperature(res); 
    Serial.print(opc.input.rtd[1]->readValue());Serial.print("  -  ");
    Serial.print(res,3);Serial.print("  -  ");
    Serial.println(temp,3);
 
}



/**
 * Cpu1 : contrôle des IT utilisateur et écran
 */
void setup1()
{

}



/**
 * Cpu1
 */
void loop1()
{

}
