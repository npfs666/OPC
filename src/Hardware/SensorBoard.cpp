#include <Hardware/SensorBoard.h>


SensorBoard::SensorBoard()
{
    numRTDSensors = 0;
    newMeasurement = false;
    pauseInterrupts = true;
}



/**
 * @brief Variables init and ADC init
 */
void SensorBoard::init()
{
    mux.begin();
    adc.begin(&SPI,SPI_CLK, SPI_MISO, SPI_MOSI, SPI_CS, SPI_DRDY);
    adc.setOpMode(0);
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
/*bool SensorBoard::addRTD(const char* name, RTDSensor::RTDType type, RTDSensor::RTDWiring wiring, uint16_t samples, float_t offset) {

    if (numRTDSensors >= MAX_RTD)
        return false;

    //rtd[numRTDSensors] = RTDSensor(name, type, wiring, samples, offset);
    numRTDSensors++;

    return true;
}*/

bool SensorBoard::addRTD(RTDSensor& sensor) {

    if (numRTDSensors >= MAX_RTD)
        return false;

    rtd[numRTDSensors] = &sensor;
    numRTDSensors++;

    return true;
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
    adc.setIDAC1routing(IDAC_AIN2);
    adc.setIDAC2routing(IDAC_AIN3_REFN1);
}



// Starts continuous conversion of the ADC
void SensorBoard::startContinuous()
{
    if (numRTDSensors == 0 || rtd[0] == nullptr) 
        return;

    // Init first read
    curRTDSensor = 0;

    setWiringRoute(rtd[curRTDSensor]->settings);

    adc.setConversionMode(CONVERSION_CONTINUOUS);
    pauseInterrupts = false;
    adc.startSync();
}



// Pauses conversion
void SensorBoard::pause() {
    pauseInterrupts = true;
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
}



// Restarts conversion
void SensorBoard::restart() {
    
    adc.setConversionMode(CONVERSION_CONTINUOUS);
    /**
     * When in pause(), any write to a registry starts a single shot conversion (and triggers interrupt), and we write many
     * so before restarting we need to clear any previous "random" measurement
     */
    //rtd[curRTDSensor].reset();
    pauseInterrupts = false;
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

    if( pauseInterrupts ) return;

    //#define PRINT_CONVERSION_TIME 
    #ifdef PRINT_CONVERSION_TIME
        uint32_t time = millis();//micros();
    #endif

    int32_t value = adc.readADC();
	
    //rtd[curRTDSensor].add(value);
    rtd[curRTDSensor]->addLP(value);

    // Cas particulier de la mesure en 3 fils (current chopping) : 
	// inversion des sources d'exitation de courant à la moitié de la série, pour supprimer leur inégalité de courant
	if ( rtd[curRTDSensor]->isAccumulationHalfWay() )
	{
        pause();
		invert3WireIDAC();
        restart();
	}

    // If all samples of one RTD are measured, compute the result
    if ( rtd[curRTDSensor]->isAccumulationDone() )
	{
        pause();

		//temperatureADC = board.ads1120.readInternalTemp();	// T°C interne de l'ADC

		rtd[curRTDSensor]->compute();

        mux.disableChannel(curRTDSensor);
		curRTDSensor++;
        //Serial.print(curRTDSensor);Serial.print("  |  ");Serial.print(numRTDSensors);
        // If all inputs RTDs are finished, we flag newMeasurement available
        if( curRTDSensor == numRTDSensors ) {
            curRTDSensor = 0;
			newMeasurement = true;
            //return; // when all measurement are done, we pause and wait for UI update to restart them.
        }

        setWiringRoute(rtd[curRTDSensor]->settings);

        // relance de la conversion continue
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

    double_t gain = 8.0;
    /*if( rtdSensor.settings.wiring == RTDSensor::RTDWiring::ThreeWire ) {
        gain = 8.0;
    } else if (rtdSensor.settings.wiring == RTDSensor::RTDWiring::FourWire ) {
        gain = 8.0;
    } else {
        gain = 1;
    }*/

    double_t Rrtd = (rtdSensor.readValue() * settings.refResistanceValue) / (32768.0 * gain);

    // Compensation de la mesure 
    // 7.5ppm mesuré (système entier) avec la diff entre la plage 24°C et 12°C
    //double_t ppm = (systemTemperature - calTemperature) * TEMPERATURE_COEFFICIENT_PPM_C;
    //Rrtd = Rrtd * (1 + ppm/1000000.0);

    return Rrtd;
}

void SensorBoard::registerParameters(ParameterList& list)
{
    for (size_t i = 0; i < numRTDSensors; i++)
    {
        rtd[i]->registerParameters(list);
    }

    auto parameters = list.forOwner({
        "calibration",
        "Calibration",
        "sensor_board",
        "Carte capteurs"
    });

    parameters.addDouble(
        "reference_resistance",
        "Résistance réf.",
        settings.refResistanceValue,
        1500.0,
        1800.0,
        0.001,
        3,
        "Ω");

    parameters.addDouble(
        "reference_coefficient",
        "Coeff. réf.",
        settings.refResistanceCoeff,
        -100.0,
        100.0,
        0.1,
        1,
        "ppm/°C");

    parameters.addDouble(
        "calibration_resistance",
        "Résistance cal.",
        settings.calResistanceValue,
        99.0,
        101.0,
        0.001,
        3,
        "Ω");

    parameters.addDouble(
        "adc_calibration_temperature",
        "Température ADC",
        settings.calTemperatureADC,
        -40.0,
        125.0,
        0.01,
        2,
        "°C");

    parameters.addDouble(
        "bme_calibration_temperature",
        "Température BME",
        settings.calTemperatureBME,
        -40.0,
        85.0,
        0.01,
        2,
        "°C");
}

void SensorBoard::resetAcquisition()
{
    pauseInterrupts = true;
    newMeasurement = false;

    for (uint8_t i = 0; i < numRTDSensors; i++)
        rtd[i]->reset();
}
