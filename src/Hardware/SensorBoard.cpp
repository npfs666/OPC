#include <Hardware/SensorBoard.h>


SensorBoard::SensorBoard()
{
    numRTDSensors = 0;
    newMeasurement = false;
}



/**
 * @brief Variables init and ADC init
 */
void SensorBoard::init()
{
    mux.begin();
    adc.begin(&SPI,SPI_CLK, SPI_MISO, SPI_MOSI, SPI_CS, SPI_DRDY);
}



/**
 * @brief Adds a sensor to the list
 * 
 * @param number id
 * @param type TYPE_2WIRE TYPE_3WIRE TYPE_4WIRE
 * @param switchPin mux pin
 * @param samples 4 samples -> 1bit improve, 16 -> 2bits, 64 -> 3bits, 256 -> 4bits (oversampling)
 * @param offset Sensor offset
 */
void SensorBoard::addRTD(RTDSensor::RTDType type, RTDSensor::RTDWiring wiring, uint16_t samples, float_t offset) {

    rtd[numRTDSensors] = RTDSensor(type, wiring, samples, offset);
    numRTDSensors++;
}



/**
 * Common config for every RTD
 */
void SensorBoard::setStandartRTD() {

    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
    adc.setMultiplexer(MUX_AINP_AIN0_AINN_AIN1);
    adc.setVoltageRef(VREF_EXTERNAL_REFP0_REFN0);
    adc.setIDAC1routing(IDAC_AIN3_REFN1);
    adc.setIDAC2routing(IDAC_DISABLED);
    adc.setFIR(FIR_50HZ); 
    adc.setDataRate(DATARATE_20_SPS);               // No 50/60Hz filtering above 20 SPS
}



/**
 * Sets up the ADC and multiplexer according each RTD settings
 */
void SensorBoard::setWiringRoute(RTDSensor::Settings settings)
{
    setStandartRTD();

    mux.enableChannel(curRTDSensor);

    switch (settings.wiring)
    {

    case RTDSensor::RTDWiring::ThreeWire:

        mux.set3Wire();
        adc.setIDAC2routing(IDAC_AIN2);
        adc.setGain(16);

        switch (settings.type)
        {

        case RTDSensor::RTDType::Pt100:
            adc.setIDACcurrent(CURRENT_500_UA);
            break;

        case RTDSensor::RTDType::Pt1000:
            adc.setIDACcurrent(CURRENT_50_UA);
            break;
        }
        break;

    case RTDSensor::RTDWiring::FourWire:

        mux.set4Wire();
        adc.setGain(8);

        switch (settings.type)
        {

        case RTDSensor::RTDType::Pt100:
            adc.setIDACcurrent(CURRENT_1000_UA);
            break;

        case RTDSensor::RTDType::Pt1000:
            adc.setIDACcurrent(CURRENT_100_UA);
            break;
        }
        break;
    default:
        break;
    }
}



/**
 * @brief Invert the IDAC current source in 3-Wire measurement, to cancel their différences (current chopping)
 * 
 */
void SensorBoard::invert3WireIDAC()
{
    adc.setConversionMode(CONVERSION_SINGLE_SHOT); // Stop conversion
    adc.setIDAC1routing(IDAC_AIN2);
    adc.setIDAC2routing(IDAC_AIN3_REFN1);
    //delay(10); // TODO : test in real life to check reading integrity
    restart();
}



// Starts continuous conversion of the ADC
void SensorBoard::startContinuous()
{
    // Init first read
    curRTDSensor = 0;

    setWiringRoute(rtd[curRTDSensor].settings);

    //delay(10);  // TODO : test in real life to check reading integrity

    adc.setConversionMode(CONVERSION_CONTINUOUS);
    adc.startSync();
}



// Pauses conversion
void SensorBoard::pause() {
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
}



// Restarts conversion
void SensorBoard::restart() {
    adc.setConversionMode(CONVERSION_CONTINUOUS);
    adc.startSync();
}



// Ne pas faire la calibration dans une interrupt, ça ne fonctionne pas avec les delays
// à caler à un endroit et terminer
void SensorBoard::calRefResistor()
{
    #define CALIBRATION_SAMPLES 64.0

    mux.enableChannel(0);
    //set4WirePT100();

    int32_t sum = 0;    

    for( uint8_t i = 0; i < CALIBRATION_SAMPLES; i++ ) {
        delay(10);
        sum += adc.readADC_Single();
    }
    mux.disableChannel(0);
    double_t readVal = sum / CALIBRATION_SAMPLES ;

    settings.refResistanceValue = (double_t) (settings.calResistanceValue * 32768.0 * 8.0 ) / readVal;
    settings.calTemperatureADC = adc.readInternalTemp();

    Serial.print(readVal,2);Serial.print(" | ");
    Serial.print(settings.refResistanceValue,3);Serial.print(" | ");
    Serial.println(settings.calTemperatureADC,2);
}

void SensorBoard::adcInterrupt() {

    //#define PRINT_CONVERSION_TIME 

    #ifdef PRINT_CONVERSION_TIME
        uint32_t time = millis();//micros();
    #endif

    int32_t value = adc.readADC();
    //Serial.println(value);
	
    rtd[curRTDSensor].add(value);

    // Cas particulier de la mesure en 3 fils (current chopping) : 
	// inversion des sources d'exitation de courant à la moitié de la série, pour supprimer leur inégalité de courant
	if ( rtd[curRTDSensor].isAccumulationHalfWay() )
	{
		invert3WireIDAC();
	}

    // If all samples of one RTD are measured, compute the result
    if ( rtd[curRTDSensor].isAccumulationDone() )
	{
        pause();

		//temperatureADC = board.ads1120.readInternalTemp();	// T°C interne de l'ADC

		rtd[curRTDSensor].compute();

        mux.disableChannel(curRTDSensor);
		curRTDSensor++;
        //Serial.print(curRTDSensor);Serial.print("  |  ");Serial.print(numRTDSensors);
        // If all inputs RTDs are finished, we flag newMeasurement available
        if( curRTDSensor == numRTDSensors ) {
            curRTDSensor = 0;
			newMeasurement = true;
        }

        setWiringRoute(rtd[curRTDSensor].settings);

        // Pause et relance de la conversion continue
		//delay(2); // TODO: uniformiser les delay
		restart();
    }

    #ifdef PRINT_CONVERSION_TIME
        Serial.println((millis()-time));
    #endif
}



/**
 * Conversion from an ADC value to a resistance
 * 
 * This is where we try to compensate the T°C derivation of the whole measurement chain. (WIP)
 * 
 * @return double_t Résistance en Ohms
 */
double_t SensorBoard::computeResistance(RTDSensor& rtdSensor) {

    #define TEMPERATURE_COEFFICIENT_PPM_C 7.5 // ancien calcul 7.5
    //#define TEMPERATURE_AT_CALIBRATION 25.4

    double_t gain = 1;
    if( rtdSensor.settings.wiring == RTDSensor::RTDWiring::ThreeWire ) {
        gain = 8.0;
    } else if (rtdSensor.settings.wiring == RTDSensor::RTDWiring::FourWire ) {
        gain = 8.0;
    }

    double_t Rrtd = (rtdSensor.readValue() * settings.refResistanceValue) / (32768.0 * gain);

    // Compensation de la mesure 
    // 7.5ppm mesuré (système entier) avec la diff entre la plage 24°C et 12°C
    //double_t ppm = (systemTemperature - calTemperature) * TEMPERATURE_COEFFICIENT_PPM_C;
    //Rrtd = Rrtd * (1 + ppm/1000000.0);

    return Rrtd;
}