#include "SensorBoard.h"
#include <Physics/PT100.h>

SensorBoard::SensorBoard()
{
    numSensors = 0;
    curSensor = 0;
    newMeasurement = false;
}

void SensorBoard::begin()
{
    adc.begin(&SPI1, SPI_CLK, SPI_MISO, SPI_MOSI, SPI_CS, SPI_DRDY);
    mux.begin();

    curSensor = 0;
    newMeasurement = false;
}

void SensorBoard::addSensor(uint8_t type,
                         uint8_t switchPin,
                         uint16_t samples,
                         float offset)
{
    if(numSensors >= MAX_RTD)
        return;

    rtd[numSensors] = Sensor(type,
                                   switchPin,
                                   samples,
                                   offset);

    numSensors++;
}

void SensorBoard::start()
{
    curSensor = 0;

    enableCurrentSensor();

    configureCurrentSensor();

    startContinuous();
}


void SensorBoard::stop()
{
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);

    // Reset input analog switches
    for( uint8_t i = 0; i < numSensors; i++ )  {
        digitalWrite(rtd[i].analogSwitchPin, LOW);
    }
}

void SensorBoard::sampleReady(int32_t value)
{
    Sensor& sensor = rtd[curSensor];
    sensor.add(value);

    if(sensor.sampleCount >= sensor.settings.samples)
    {
        finishCurrentSensor();
    }
}

void SensorBoard::onDataReady()
{
    sampleReady(adc.readADC());
}

bool SensorBoard::available() const
{
    return newMeasurement;
}

uint8_t SensorBoard::sensorCount() const
{
    return numSensors;
}

static double adcCountsToResistance(double counts, double gain, double current_uA)
{
    const double refVoltage = 2.048;
    const double fullScale = refVoltage / gain;
    const double currentA = current_uA / 1000000.0;
    return (counts / 32768.0) * fullScale / currentA;
}

double SensorBoard::getRTDResistance(uint8_t index)
{
    if(index >= numSensors)
        return NAN;

    double gain = 8.0;
    double current_uA = 1000.0;

    if(rtd[index].measurementType == TYPE_3WIRE)
    {
        gain = 16.0;
        current_uA = 500.0;
    }

    return adcCountsToResistance(static_cast<double>(rtd[index].readValue()), gain, current_uA);
}

double SensorBoard::getRTDTemperature(uint8_t index)
{
    double resistance = getRTDResistance(index);
    return PT100::getResistanceToTemperature(resistance);
}

void SensorBoard::finishCurrentSensor()
{
    Sensor& sensor = rtd[curSensor];

    stop();

    temperatureADC = adc.readInternalTemp();

    sensor.compute();

    disableCurrentSensor();

    curSensor++;

    if(curSensor >= numSensors)
    {
        curSensor = 0;
        newMeasurement = true;
    }

    enableCurrentSensor();

    configureCurrentSensor();

    delay(2);

    restart();
}

void SensorBoard::enableCurrentSensor()
{
    digitalWrite(rtd[curSensor].analogSwitchPin, HIGH);
}

void SensorBoard::disableCurrentSensor()
{
    digitalWrite(rtd[curSensor].analogSwitchPin, LOW);
}

void SensorBoard::configureCurrentSensor()
{
    switch(rtd[curSensor].measurementType)
    {
        case TYPE_3WIRE:
            set3WirePT100();
            break;

        case TYPE_4WIRE:
            set4WirePT100();
            break;
    }
}

/**
 * @brief Configures the ADC to measure a 4-Wire PT-100 RTD
 *
 */
void SensorBoard::set4WirePT100()
{
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
    digitalWrite(SW_3_WIRE, LOW);
    digitalWrite(SW_4_WIRE, HIGH);
    adc.setMultiplexer(MUX_AINP_AIN0_AINN_AIN1);
    adc.setFIR(FIR_50HZ);
    adc.setVoltageRef(VREF_EXTERNAL_REFP0_REFN0);
    adc.setIDAC1routing(IDAC_AIN3_REFN1);
    adc.setIDACcurrent(CURRENT_1000_UA);
    adc.setGain(8);
}
/**
 * @brief Configures the ADC to measure a 3-Wire PT-100 RTD
 *
 */
void SensorBoard::set3WirePT100()
{
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
    digitalWrite(SW_3_WIRE, HIGH);
    digitalWrite(SW_4_WIRE, LOW);
    adc.setMultiplexer(MUX_AINP_AIN0_AINN_AIN1);
    adc.setFIR(FIR_50HZ);
    adc.setVoltageRef(VREF_EXTERNAL_REFP0_REFN0);
    adc.setIDAC1routing(IDAC_AIN3_REFN1);
    adc.setIDAC2routing(IDAC_AIN2);
    adc.setIDACcurrent(CURRENT_500_UA);
    adc.setGain(16);
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
    delay(10);
    restart();
}

// Restarts conversion
void SensorBoard::restart() {
    adc.setConversionMode(CONVERSION_CONTINUOUS);
    adc.startSync();
}

// Starts continuous conversion of the ADC
void SensorBoard::startContinuous()
{
    // Init first read
    curSensor = 0;
    digitalWrite(rtd[curSensor].analogSwitchPin, HIGH);  // Input analog switch ON
    if (rtd[curSensor].measurementType == TYPE_3WIRE) {
        set3WirePT100();
    } else if(rtd[curSensor].measurementType == TYPE_4WIRE) {
        set4WirePT100();
    }

    delay(10);
    adc.setDataRate(DATARATE_20_SPS);   // No 50/60Hz filtering above 20 SPS
    adc.setConversionMode(CONVERSION_CONTINUOUS);
    adc.startSync();
}