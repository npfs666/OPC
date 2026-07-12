#include<Hardware/SensorBoard.h>
#include<Arduino.h>


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

void SensorBoard::setBasicRTD() {

    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
    adc.setMultiplexer(MUX_AINP_AIN0_AINN_AIN1);
    adc.setFIR(FIR_50HZ); 
    adc.setVoltageRef(VREF_EXTERNAL_REFP0_REFN0);
    adc.setIDAC1routing(IDAC_AIN3_REFN1);
    adc.setIDAC2routing(IDAC_DISABLED);
}

void SensorBoard::setWiringRoute(RTDSensor::Settings settings)
{
    setBasicRTD();

    switch (settings.wiring)
    {

    case RTDSensor::RTDWiring::Wire3:

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

    case RTDSensor::RTDWiring::Wire4:

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

    mux.enableChannel(curRTDSensor);

    setWiringRoute(rtd[curRTDSensor].settings);

    //delay(10);  // TODO : test in real life to check reading integrity

    adc.setDataRate(DATARATE_20_SPS);               // No 50/60Hz filtering above 20 SPS
    adc.setConversionMode(CONVERSION_CONTINUOUS);
    adc.startSync();
}

// Pauses conversion
void SensorBoard::pause() {
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);

    // Reset input analog switches
    /*for( uint8_t i = 0; i < numRTDSensors; i++ )  {
        mux.disableChannel(i);
    }*/
}

// Restarts conversion
void SensorBoard::restart() {
    adc.setConversionMode(CONVERSION_CONTINUOUS);
    adc.startSync();
}

// Resets RTD sums
/*void SensorBoard::resetCounts() {
    for( uint8_t i = 0; i < numRTDSensors; i++ )  {
        rtd[i].reset();
    }
}*/

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
    settings.calTemperature = adc.readInternalTemp();

    Serial.print(readVal,2);Serial.print(" | ");
    Serial.print(settings.refResistanceValue,3);Serial.print(" | ");
    Serial.println(settings.calTemperature,2);
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

        mux.enableChannel(curRTDSensor);
        setWiringRoute(rtd[curRTDSensor].settings);

        // Pause et relance de la conversion continue
		//delay(2); // TODO: uniformiser les delay
		restart();
    }

    #ifdef PRINT_CONVERSION_TIME
        Serial.println((millis()-time));
    #endif
}
// pas ici
/*void SensorBoard::convertToTemperature(float_t systemTemperature) {

    // Navigation dans les sondes déclarées
    for( uint8_t i = 0; i < this->numRTDSensors; i++ ) {
      
        rtd[i].resistance = getResistanceValue(i, systemTemperature);
        rtd[i].temperature = getRTDTempInterpolation(i);
    } 
}*/

//void SensorBoard::compute

/**
 * @brief Convertit une valeur entière sur 16bits en Resistance en fonction
 * de la valeur de la résistance de calibration refResistanceValue.
 * 
 * @param rtdSensor sensor index
 * @param systemTemperature température actuelle du système
 * @return double_t Résistance en Ohms
 */
double_t SensorBoard::getResistanceValue(uint8_t rtdSensor) {

    #define TEMPERATURE_COEFFICIENT_PPM_C 7.5 // ancien calcul 7.5
    //#define TEMPERATURE_AT_CALIBRATION 25.4

    double_t gain = 1;
    if( rtd[rtdSensor].settings.wiring == RTDSensor::RTDWiring::Wire3 ) {
        gain = 8.0;
    } else if (rtd[rtdSensor].settings.wiring == RTDSensor::RTDWiring::Wire4 ) {
        gain = 8.0;
    }

    double_t Rrtd = (rtd[rtdSensor].readValue() * settings.refResistanceValue) / (32768.0 * gain);

    // Compensation de la mesure 
    // 7.5ppm mesuré (système entier) avec la diff entre la plage 24°C et 12°C
    //double_t ppm = (systemTemperature - calTemperature) * TEMPERATURE_COEFFICIENT_PPM_C;
    //Rrtd = Rrtd * (1 + ppm/1000000.0);

    return Rrtd;
}

double_t SensorBoard::computeResistance(RTDSensor& rtdSensor) {

    #define TEMPERATURE_COEFFICIENT_PPM_C 7.5 // ancien calcul 7.5
    //#define TEMPERATURE_AT_CALIBRATION 25.4

    double_t gain = 1;
    if( rtdSensor.settings.wiring == RTDSensor::RTDWiring::Wire3 ) {
        gain = 8.0;
    } else if (rtdSensor.settings.wiring == RTDSensor::RTDWiring::Wire4 ) {
        gain = 8.0;
    }

    double_t Rrtd = (rtdSensor.readValue() * settings.refResistanceValue) / (32768.0 * gain);

    // Compensation de la mesure 
    // 7.5ppm mesuré (système entier) avec la diff entre la plage 24°C et 12°C
    //double_t ppm = (systemTemperature - calTemperature) * TEMPERATURE_COEFFICIENT_PPM_C;
    //Rrtd = Rrtd * (1 + ppm/1000000.0);

    return Rrtd;
}

/**
 * @brief Conversion d'une resistance en température via le calcul par interpolation (plus précis qu'une fonction pour une approche réelle)
 * 
 * @param id sensor index
 * @return double_t temperature in °C
 */
// pas ici
/*double_t SensorBoard::getRTDTempInterpolation(uint8_t id) {

    double_t Rrtd = rtd[id].resistance;

    int16_t index=(int16_t) (Rrtd/10);
    double_t frac = (double_t)(Rrtd/10.0) - index;
    double_t a = rtdInterpol[index];

    double_t temperature = 0;

    // Si valeur juste, on lis la case directement
    if (index == Rrtd / 10)
    {
        temperature = rtdInterpol[index];
    }
    // Sinon approximation par interpolation du des valeurs du tableau
    double_t b = rtdInterpol[index + 1] / 2.0;
    double_t c = rtdInterpol[index - 1] / 2.0;
    temperature = (double_t)a + frac * (b - c + frac * (c + b - a));

    return temperature + rtd[id].offset;
}*/

/**
 * @brief Calcule l'humidité relative à partir d'une température sèche et humide
 * 
 * @param tempSeche température de la sonde sèche en °C
 * @param tempHumide température du bulbe humide en °C
 * @param pressionAtm en Pa
 * @return double_t Humidité relative en %
 */
/* pas ici
double_t SensorBoard::getRH(uint8_t dryID, uint8_t wetID, double_t atmPressure) {

    double_t dryTemperature = rtd[dryID].temperature;
    double_t wetTemperature = rtd[wetID].temperature;

    // 1: Calcul de la "constante" psychrométrique
    // Capacité thermique massique de l'air [kJ/kg.°C]
    // 0.00006 * tempSeche + 1.005; ancien calcul
    double_t Cp = (3 * dryTemperature)/50000.0 + 1.005;
    // Energie de vaporiation de l'eau [kJ/kg]
    double_t lambda = -2.3664 * wetTemperature + 2501;
    double_t A = Cp / (lambda * 0.622 ); // [1/°C]

    // Atmospheric pressure [kPa]
	double_t P = atmPressure / 1000.0F;

    double_t pVs = 0.6108 * pow(2.71828, ((17.27 * wetTemperature)/(wetTemperature + 237.3))); // [kPa]
    double_t pV = pVs - A*P*(dryTemperature-wetTemperature); // [kPa]
    double_t pVs2 = 0.6108 * pow(2.71828, ((17.27 * dryTemperature)/(dryTemperature + 237.3))); // [kPa]

    // Ancien calcul
    //double_t pVs = pow(10,(2.7877+(7.625*mes1)/(241.6+mes1)));
    //double_t pV = pVs - 0.000667*102.3000*(mes2-mes1);
    //double_t pVs2 = pow(10,(2.7877+(7.625*mes2)/(241.6+mes2)));

    double_t rh = ((double_t)pV/pVs2)*100.0;

    return rh;
}
    */