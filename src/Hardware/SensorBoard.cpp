#include <Hardware/SensorBoard.h>

#include <hardware/gpio.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <limits>


namespace
{
    static_assert(
        MAX_RTD == 3,
        "Calibration menu actions assume three RTD inputs");

    constexpr uint8_t CALIBRATION_CHANNEL = 0;
    constexpr uint8_t CALIBRATION_DISCARDED_SAMPLES = 4;
    constexpr uint8_t CALIBRATION_SAMPLES = 64;
    constexpr uint32_t CALIBRATION_SAMPLE_TIMEOUT_MS = 250;
    constexpr int32_t ADC_CALIBRATION_LIMIT = 32000;
    constexpr int32_t MAX_ZERO_OFFSET_COUNTS = 2048;
    constexpr int32_t MAX_SAMPLE_SPREAD_COUNTS = 256;
    constexpr double_t REFERENCE_TOLERANCE = 0.05;
    constexpr double_t ADC_FULL_SCALE = 32768.0;
}


SensorBoard::SensorBoard()
{
    beginConfiguration("sensor_board");
    numRTDSensors = 0;
    newMeasurement = false;
    pauseInterrupts = true;
    adcTemperature = 0.0;

    for (uint8_t i = 0; i < MAX_RTD; i++)
        rtd[i] = nullptr;
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

    pauseInterrupts = true;

    // Init first read
    curRTDSensor = 0;

    setWiringRoute(rtd[curRTDSensor]->settings);

    adc.setConversionMode(CONVERSION_CONTINUOUS);
    discardNextConversion = true;
    adc.startSync();

    gpio_acknowledge_irq(SPI_DRDY, GPIO_IRQ_EDGE_FALL);

    pauseInterrupts = false;
}



// Pauses conversion
void SensorBoard::pause() {
    pauseInterrupts = true;
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
}



// Restarts conversion
void SensorBoard::restart() {

    adc.setConversionMode(CONVERSION_CONTINUOUS);
    discardNextConversion = true;

    /**
     * Une conversion synchrone, notamment la température interne de l'ADC,
     * peut laisser un front DRDY en attente. Il doit être acquitté avant
     * d'accepter les mesures du canal suivant.
     */
    adc.startSync();

    gpio_acknowledge_irq(SPI_DRDY, GPIO_IRQ_EDGE_FALL);

    pauseInterrupts = false;
}



void SensorBoard::adcInterrupt() {

    if( pauseInterrupts ) return;

    //#define PRINT_CONVERSION_TIME 
    #ifdef PRINT_CONVERSION_TIME
        uint32_t time = millis();//micros();
    #endif

    int32_t value = adc.readADC();

    if (discardNextConversion)
    {
        discardNextConversion = false;
        return;
    }
	
    //rtd[curRTDSensor].add(value);
    rtd[curRTDSensor]->add(value);

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
            adcTemperature = adc.readInternalTemp();
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
    const uint8_t channel = channelFor(rtdSensor);

    if (channel >= MAX_RTD)
        return NAN;

    const CalibrationProfile& calibration =
        calibrationFor(rtdSensor.settings.type);

    const double_t gain =
        measurementGain(rtdSensor.settings.wiring);

    const double_t correctedValue =
        rtdSensor.readValue() -
        settings.zeroOffset[channel];

    double_t Rrtd =
        correctedValue *
        calibration.refResistanceValue /
        (ADC_FULL_SCALE * gain);
    
    // Compensation de la mesure en fonction de la température
    const double_t ppm =
        (adcTemperature - calibration.calTemperatureADC) *
        calibration.systemPPMCoeff;

    Rrtd = Rrtd * (1 + ppm/1000000.0);

    return Rrtd;
}

void SensorBoard::registerParameters(ParameterList& list)
{
    for (size_t i = 0; i < numRTDSensors; i++)
    {
        rtd[i]->registerParameters(list);
    }

    registerZeroCalibrationParameters(list);

    registerCalibrationParameters(
        list,
        RTDSensor::RTDType::Pt100);

    registerCalibrationParameters(
        list,
        RTDSensor::RTDType::Pt1000);
}

void SensorBoard::registerCalibrationParameters(
    ParameterList& list,
    RTDSensor::RTDType type)
{
    const bool isPt1000 =
        type == RTDSensor::RTDType::Pt1000;

    CalibrationProfile& calibration =
        calibrationFor(type);

    const double_t nominalReference =
        nominalReferenceResistance(type);

    const double_t nominalCalibrationResistance =
        isPt1000 ? 1000.0 : 100.0;

    auto parameters = list.forOwner({
        "calibration",
        "Calibration",
        isPt1000
            ? "sensor_board.pt1000_calibration"
            : "sensor_board.pt100_calibration",
        isPt1000 ? "PT1000" : "PT100"
    });

    parameters.addDouble(
        "reference_resistance",
        "Rref",
        calibration.refResistanceValue,
        nominalReference * 0.9,
        nominalReference * 1.1,
        0.001,
        3,
        "Ω",
        true);

    parameters.addDouble(
        "reference_coefficient",
        "Coeff",
        calibration.systemPPMCoeff,
        -50.0,
        50.0,
        0.1,
        1,
        "ppm/°C");

    parameters.addDouble(
        "calibration_resistance",
        "Rcal.",
        calibration.calResistanceValue,
        nominalCalibrationResistance * 0.9,
        nominalCalibrationResistance * 1.1,
        0.001,
        3,
        "Ω");

    parameters.addDouble(
        "adc_calibration_temperature",
        "adcCalTemp",
        calibration.calTemperatureADC,
        -50.0,
        150.0,
        0.01,
        2,
        "°C",
        true);

}

void SensorBoard::registerZeroCalibrationParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "calibration",
        "Calibration",
        "sensor_board.zero_calibration",
        "Zeros ADC"
    });

    constexpr const char* ZERO_KEYS[MAX_RTD] = {
        "zero_input_1",
        "zero_input_2",
        "zero_input_3"
    };

    constexpr const char* ZERO_NAMES[MAX_RTD] = {
        "N0 entree 1",
        "N0 entree 2",
        "N0 entree 3"
    };

    for (uint8_t channel = 0;
         channel < MAX_RTD;
         channel++)
    {
        parameters.addDouble(
            ZERO_KEYS[channel],
            ZERO_NAMES[channel],
            settings.zeroOffset[channel],
            -MAX_ZERO_OFFSET_COUNTS,
            MAX_ZERO_OFFSET_COUNTS,
            0.01,
            2,
            "pts",
            true);
    }
}

bool SensorBoard::addMenuActions(
    MenuBuilder& menu) const
{
    const MenuBuilder::GroupId pt100Group =
        menu.findGroupForOwner(
            "sensor_board.pt100_calibration");

    const MenuBuilder::GroupId pt1000Group =
        menu.findGroupForOwner(
            "sensor_board.pt1000_calibration");

    const MenuBuilder::GroupId zeroGroup =
        menu.findGroupForOwner(
            "sensor_board.zero_calibration");

    if (pt100Group == MenuBuilder::INVALID_GROUP &&
        pt1000Group == MenuBuilder::INVALID_GROUP &&
        zeroGroup == MenuBuilder::INVALID_GROUP)
    {
        return true;
    }

    if (pt100Group == MenuBuilder::INVALID_GROUP ||
        pt1000Group == MenuBuilder::INVALID_GROUP ||
        zeroGroup == MenuBuilder::INVALID_GROUP)
    {
        return false;
    }

    return
        menu.addAction(
            zeroGroup,
            CALIBRATE_ZERO_INPUT_1,
            "zero_input_1",
            "Mesurer N0 E1") &&
        menu.addAction(
            zeroGroup,
            CALIBRATE_ZERO_INPUT_2,
            "zero_input_2",
            "Mesurer N0 E2") &&
        menu.addAction(
            zeroGroup,
            CALIBRATE_ZERO_INPUT_3,
            "zero_input_3",
            "Mesurer N0 E3") &&
        menu.addAction(
            zeroGroup,
            RESET_ZERO_OFFSETS,
            "reset_zero_offsets",
            "RAZ des N0") &&
        menu.addAction(
            pt100Group,
            CALIBRATE_PT100_REFERENCE,
            "pt100_reference",
            "Calibrer Rref (E1)") &&
        menu.addAction(
            pt1000Group,
            CALIBRATE_PT1000_REFERENCE,
            "pt1000_reference",
            "Calibrer Rref (E1)");
}

bool SensorBoard::handlesMenuAction(
    MenuBuilder::ActionId actionId) const
{
    return
        actionId >= CALIBRATE_ZERO_INPUT_1 &&
        actionId <= RESET_ZERO_OFFSETS;
}

bool SensorBoard::executeMenuAction(
    MenuBuilder::ActionId actionId)
{
    if (!handlesMenuAction(actionId))
        return false;

    calibrationBackup = settings;
    calibrationBackupValid = true;

    bool success = false;

    if (actionId >= CALIBRATE_ZERO_INPUT_1 &&
        actionId <= CALIBRATE_ZERO_INPUT_3)
    {
        success = calibrateZero(
            actionId - CALIBRATE_ZERO_INPUT_1);
    }
    else if (actionId == CALIBRATE_PT100_REFERENCE)
    {
        success = calibrateReference(
            RTDSensor::RTDType::Pt100,
            CALIBRATION_CHANNEL);
    }
    else if (actionId == CALIBRATE_PT1000_REFERENCE)
    {
        success = calibrateReference(
            RTDSensor::RTDType::Pt1000,
            CALIBRATION_CHANNEL);
    }
    else if (actionId == RESET_ZERO_OFFSETS)
    {
        success = resetZeroOffsets();
    }

    if (!success)
    {
        settings = calibrationBackup;
        calibrationBackupValid = false;
    }

    return success;
}

void SensorBoard::onMenuActionSaved(
    MenuBuilder::ActionId actionId)
{
    if (handlesMenuAction(actionId))
        calibrationBackupValid = false;
}

void SensorBoard::onMenuActionSaveFailed(
    MenuBuilder::ActionId actionId)
{
    if (handlesMenuAction(actionId) &&
        calibrationBackupValid)
    {
        settings = calibrationBackup;
        calibrationBackupValid = false;
    }
}

SensorBoard::CalibrationProfile&
SensorBoard::calibrationFor(
    RTDSensor::RTDType type)
{
    return
        type == RTDSensor::RTDType::Pt1000
            ? settings.pt1000
            : settings.pt100;
}

const SensorBoard::CalibrationProfile&
SensorBoard::calibrationFor(
    RTDSensor::RTDType type) const
{
    return
        type == RTDSensor::RTDType::Pt1000
            ? settings.pt1000
            : settings.pt100;
}

uint8_t SensorBoard::channelFor(
    const RTDSensor& sensor) const
{
    for (uint8_t channel = 0;
         channel < numRTDSensors;
         channel++)
    {
        if (rtd[channel] == &sensor)
            return channel;
    }

    return MAX_RTD;
}

bool SensorBoard::calibrateZero(uint8_t channel)
{
    CalibrationSamples samples;

    if (!readCalibrationSamples(
            RTDSensor::RTDType::Pt100,
            channel,
            samples))
    {
        return false;
    }

    if (std::fabs(samples.average) >
            MAX_ZERO_OFFSET_COUNTS)
    {
        Serial.println(
            "Calibration N0 rejected: offset too large");
        return false;
    }

    settings.zeroOffset[channel] =
        samples.average;

    Serial.print("Calibration N0 input ");
    Serial.print(channel + 1);
    Serial.print(": ");
    Serial.println(samples.average, 2);

    return true;
}

bool SensorBoard::resetZeroOffsets()
{
    for (uint8_t channel = 0;
         channel < MAX_RTD;
         channel++)
    {
        settings.zeroOffset[channel] = 0.0;
    }

    Serial.println("ADC zero offsets reset");
    return true;
}

bool SensorBoard::calibrateReference(
    RTDSensor::RTDType type,
    uint8_t channel)
{
    CalibrationSamples samples;

    if (!readCalibrationSamples(
            type,
            channel,
            samples))
    {
        return false;
    }

    CalibrationProfile& calibration =
        calibrationFor(type);

    const double_t correctedValue =
        samples.average -
        settings.zeroOffset[channel];

    if (!(correctedValue > 0.0) ||
        correctedValue >= ADC_CALIBRATION_LIMIT)
    {
        Serial.println(
            "Rref calibration rejected: invalid ADC value");
        return false;
    }

    const double_t calibratedReference =
        calibration.calResistanceValue *
        ADC_FULL_SCALE *
        measurementGain(
            RTDSensor::RTDWiring::FourWire) /
        correctedValue;

    const double_t nominalReference =
        nominalReferenceResistance(type);

    if (!std::isfinite(calibratedReference) ||
        calibratedReference <
            nominalReference *
                (1.0 - REFERENCE_TOLERANCE) ||
        calibratedReference >
            nominalReference *
                (1.0 + REFERENCE_TOLERANCE))
    {
        Serial.println(
            "Rref calibration rejected: value out of range");
        return false;
    }

    double_t adcCalibrationTemperature = 0.0;

    if (!adc.readInternalTemp(
            adcCalibrationTemperature,
            CALIBRATION_SAMPLE_TIMEOUT_MS) ||
        !std::isfinite(adcCalibrationTemperature) ||
        adcCalibrationTemperature < -50.0 ||
        adcCalibrationTemperature > 150.0)
    {
        Serial.println(
            "Rref calibration rejected: invalid ADC temperature");
        return false;
    }

    calibration.refResistanceValue =
        calibratedReference;
    calibration.calTemperatureADC =
        adcCalibrationTemperature;

    Serial.print("Rref calibrated: ");
    Serial.print(calibration.refResistanceValue, 3);
    Serial.print(" ohm at ");
    Serial.print(calibration.calTemperatureADC, 2);
    Serial.println(" C");

    return true;
}

bool SensorBoard::readCalibrationSamples(
    RTDSensor::RTDType type,
    uint8_t channel,
    CalibrationSamples& samples)
{
    if (channel >= MAX_RTD)
        return false;

    resetAcquisition();
    curRTDSensor = channel;

    RTDSensor::Settings calibrationSettings{
        type,
        RTDSensor::RTDWiring::FourWire,
        0.0,
        CALIBRATION_SAMPLES
    };

    setWiringRoute(calibrationSettings);

    int32_t value = 0;

    for (uint8_t i = 0;
         i < CALIBRATION_DISCARDED_SAMPLES;
         i++)
    {
        if (!adc.readADC_Single(
                value,
                CALIBRATION_SAMPLE_TIMEOUT_MS))
        {
            stopCalibrationHardware(channel);
            Serial.println(
                "Calibration failed: ADC timeout");
            return false;
        }
    }

    int64_t sum = 0;
    int32_t minimum =
        std::numeric_limits<int32_t>::max();
    int32_t maximum =
        std::numeric_limits<int32_t>::lowest();

    for (uint8_t i = 0;
         i < CALIBRATION_SAMPLES;
         i++)
    {
        if (!adc.readADC_Single(
                value,
                CALIBRATION_SAMPLE_TIMEOUT_MS))
        {
            stopCalibrationHardware(channel);
            Serial.println(
                "Calibration failed: ADC timeout");
            return false;
        }

        sum += value;

        if (value < minimum)
            minimum = value;

        if (value > maximum)
            maximum = value;
    }

    stopCalibrationHardware(channel);

    if (minimum <= -ADC_CALIBRATION_LIMIT ||
        maximum >= ADC_CALIBRATION_LIMIT ||
        (maximum - minimum) >
            MAX_SAMPLE_SPREAD_COUNTS)
    {
        Serial.println(
            "Calibration rejected: unstable or saturated input");
        return false;
    }

    samples.average =
        static_cast<double_t>(sum) /
        CALIBRATION_SAMPLES;

    return true;
}

void SensorBoard::stopCalibrationHardware(
    uint8_t channel)
{
    mux.disableChannel(channel);
    adc.setIDACcurrent(CURRENT_0_UA);
    adc.setIDAC1routing(IDAC_DISABLED);
    adc.setIDAC2routing(IDAC_DISABLED);
}

double_t SensorBoard::nominalReferenceResistance(
    RTDSensor::RTDType type)
{
    return
        type == RTDSensor::RTDType::Pt1000
            ? 16500.0
            : 1650.0;
}

double_t SensorBoard::measurementGain(
    RTDSensor::RTDWiring wiring)
{
    switch (wiring)
    {
    case RTDSensor::RTDWiring::ThreeWire:
    case RTDSensor::RTDWiring::FourWire:
        return 8.0;

    case RTDSensor::RTDWiring::TwoWire:
    default:
        return 1.0;
    }
}

void SensorBoard::resetAcquisition()
{
    pauseInterrupts = true;
    newMeasurement = false;
    discardNextConversion = false;

    for (uint8_t i = 0; i < MAX_RTD; i++)
        mux.disableChannel(i);

    adc.setIDACcurrent(CURRENT_0_UA);
    adc.setIDAC1routing(IDAC_DISABLED);
    adc.setIDAC2routing(IDAC_DISABLED);

    curRTDSensor = 0;

    for (uint8_t i = 0; i < numRTDSensors; i++)
        rtd[i]->reset();
}
