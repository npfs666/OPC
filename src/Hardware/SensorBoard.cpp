// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/SensorBoard.h>

#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <limits>


namespace
{
    static_assert(
        MAX_RTD == 3,
        "Calibration menu actions assume three RTD inputs");

    constexpr uint8_t CALIBRATION_CHANNEL = 0;
    // Même débit et même suréchantillonnage que les mesures (voir Sensor.h)
    constexpr uint16_t CALIBRATION_DISCARDED_SAMPLES = RTD_DISCARDED_CONVERSIONS;
    constexpr uint16_t CALIBRATION_SAMPLES = 64 * RTD_OVERSAMPLING;
    constexpr uint32_t CALIBRATION_SAMPLE_TIMEOUT_MS = 250;
    constexpr int32_t ADC_CALIBRATION_LIMIT = 32000;
    constexpr int32_t MAX_ZERO_OFFSET_COUNTS = 2048;
    constexpr int32_t MAX_SAMPLE_SPREAD_COUNTS = 256;
    constexpr double_t REFERENCE_TOLERANCE = 0.05;
    constexpr double_t ADC_FULL_SCALE = 32768.0;

    // Calibrations propres à la carte : /board.json, voir ParameterOwner.
    ParameterOwner boardOwner(
        const char* ownerKey,
        const char* ownerName,
        bool persistent = true)
    {
        ParameterOwner owner{
            "calibration", "Calibration", ownerKey, ownerName, persistent};
        owner.board = true;
        return owner;
    }
}


SensorBoard::SensorBoard()
    : sequencer(*this)
{
    beginConfiguration("sensor_board");
    numSensors = 0;
    newMeasurement = false;
    adcTemperature = NAN;

    for (uint8_t i = 0; i < MAX_RTD; i++)
        rtd[i] = nullptr;
}



/**
 * @brief Variables init and ADC init
 *
 * @return false si le MCP23017 des multiplexeurs ne répond pas
 */
bool SensorBoard::init()
{
    const bool muxReady = mux.begin(Wire1);

    // L'ADC est initialisé dans tous les cas pour laisser le SPI dans un état connu.
    adc.begin(Board::Rp2040::ADC_SPI,
                Board::Rp2040::ADC_CLK,
                Board::Rp2040::ADC_MISO,
                Board::Rp2040::ADC_MOSI,
                Board::Rp2040::ADC_CS,
                Board::Rp2040::ADC_DRDY);
    adc.setOpMode(0);

    return muxReady;
}




bool SensorBoard::addSensor(Sensor& sensor) {

    if (sensor.board != nullptr || numSensors >= MAX_RTD)
        return false;

    sensor.board = this;
    rtd[numSensors] = &sensor;
    numSensors++;

    return true;
}



/**
 * Common config for every RTD
 */
void SensorBoard::setStandardRTD() {

    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
    adc.setMultiplexer(MUX_AINP_AIN0_AINN_AIN1);
    adc.setVoltageRef(VREF_EXTERNAL_REFP0_REFN0);
    adc.setIDAC1routing(IDAC_AIN3_REFN1);
    adc.setIDAC2routing(IDAC_DISABLED);
    adc.setFIR(FIR_NONE);
    adc.setDataRate(DATARATE_1000_SPS); // Dithering, voir RTD_OVERSAMPLING (Sensor.h)
}

void SensorBoard::setStandardTC() {

    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
    adc.setMultiplexer(MUX_AINP_AIN0_AINN_AIN1);
    adc.setVoltageRef(VREF_INTERNAL_2048_MV);
    adc.setIDAC1routing(IDAC_DISABLED);
    adc.setIDAC2routing(IDAC_DISABLED);
    adc.setIDACcurrent(CURRENT_0_UA);
    adc.setFIR(FIR_50HZ); 
    adc.setDataRate(DATARATE_20_SPS);   // No 50/60Hz filtering above 20 SPS
}


/**
 * Montage et référence d'abord, voie coupée, puis configuration de l'ADC
 * selon le montage. La voie est connectée en dernier.
 */
void SensorBoard::routeInput(
    const Sensor::Settings& inputSettings,
    uint8_t channel)
{
    Sensor::Settings settings = inputSettings;

    mux.write(AnalogRoute::forInput(settings));

    if (settings.type == Sensor::Type::Tc)
        settings.wiring = Sensor::Wiring::TwoWire;

    switch (settings.wiring)
    {
    case Sensor::Wiring::TwoWire:

        switch (settings.type)
        {

        case Sensor::Type::Pt100:
            adc.setGain(Sensor::adcGain(settings));
            setStandardRTD();
            adc.setIDACcurrent(CURRENT_1000_UA);
            break;

        case Sensor::Type::Pt1000:
            adc.setGain(Sensor::adcGain(settings));
            setStandardRTD();
            adc.setIDACcurrent(CURRENT_100_UA);
            break;

        case Sensor::Type::Tc:
            adc.setGain(thermocoupleGain(settings.thermocoupleType));
            setStandardTC();
        }
        break;

    case Sensor::Wiring::ThreeWire:

        setStandardRTD();
        adc.setIDAC2routing(IDAC_AIN2);
        adc.setGain(Sensor::adcGain(settings));

        switch (settings.type)
        {

        case Sensor::Type::Pt100:
            adc.setIDACcurrent(CURRENT_500_UA);
            break;

        case Sensor::Type::Pt1000:
            adc.setIDACcurrent(CURRENT_50_UA);
            break;
        }
        break;

    case Sensor::Wiring::FourWire:

        setStandardRTD();
        adc.setGain(Sensor::adcGain(settings));

        switch (settings.type)
        {

        case Sensor::Type::Pt100:
            adc.setIDACcurrent(CURRENT_1000_UA);
            break;

        case Sensor::Type::Pt1000:
            adc.setIDACcurrent(CURRENT_100_UA);
            break;
        }
        break;
    default:
        break;
    }

    mux.write(AnalogRoute::forInput(settings, channel));
}

void SensorBoard::disconnectInput(const Sensor::Settings& inputSettings)
{
    mux.write(AnalogRoute::forInput(inputSettings));
}

void SensorBoard::stopConversions()
{
    adc.setConversionMode(CONVERSION_SINGLE_SHOT);
}

/**
 * 3 fils : inversion des sources de courant, pour supprimer leur inégalité
 * (current chopping).
 */
void SensorBoard::invertCurrents()
{
    adc.setIDAC1routing(IDAC_AIN2);
    adc.setIDAC2routing(IDAC_AIN3_REFN1);
}

void SensorBoard::selectExtendedGain(const Sensor::Settings& inputSettings)
{
    Sensor::Settings extended = inputSettings;
    extended.range = Sensor::Range::Extended;

    adc.setGain(Sensor::adcGain(extended));
}

void SensorBoard::beginTemperature()
{
    adc.setTemperatureMode(TEMP_ON);
}

void SensorBoard::endTemperature(int32_t raw)
{
    adc.setTemperatureMode(TEMP_OFF);
    adcTemperature = ADS1120::rawToTemperature(raw);
}

uint32_t SensorBoard::disableInterrupts()
{
    return save_and_disable_interrupts();
}

void SensorBoard::restoreInterrupts(uint32_t state)
{
    restore_interrupts(state);
}

void SensorBoard::restartConversions(bool continuous)
{
    adc.setConversionMode(
        continuous ? CONVERSION_CONTINUOUS : CONVERSION_SINGLE_SHOT);

    /*
     * Une conversion faite avant la relance, notamment une conversion
     * unique, peut laisser un front DRDY en attente : il est acquitté.
     */
    adc.startSync();
    gpio_acknowledge_irq(Board::Rp2040::ADC_DRDY, GPIO_IRQ_EDGE_FALL);
}

void SensorBoard::startAcquisition()
{
    stopAcquisition();
    sequencer.start(rtd, numSensors);
}

void SensorBoard::stopAcquisition()
{
    sequencer.stop();
    newMeasurement = false;

    adc.setConversionMode(CONVERSION_SINGLE_SHOT);

    // Une mesure de température ADC interrompue ne doit pas laisser l'ADC
    // en mode capteur interne pour le cycle suivant.
    adc.setTemperatureMode(TEMP_OFF);

    mux.write(AnalogRoute{});

    adc.setIDACcurrent(CURRENT_0_UA);
    adc.setIDAC1routing(IDAC_DISABLED);
    adc.setIDAC2routing(IDAC_DISABLED);

    for (uint8_t i = 0; i < numSensors; i++)
        rtd[i]->reset();
}

void SensorBoard::poll()
{
    sequencer.poll();

    if (sequencer.takeCycleCompleted())
        newMeasurement = true;
}

void SensorBoard::adcInterrupt()
{
    if (!sequencer.isArmed())
        return;

    /*
     * Seul DRDY bas garantit une donnée neuve. Quand l'ISR a été retardée
     * (écriture flash, pile USB), une IRQ peut suivre la précédente sans
     * nouvelle conversion : DRDY est déjà haut et l'ADS1120 renvoie 0xFFFF.
     * Lu, ce -1 serait accumulé comme un échantillon (-1/800 sur la moyenne,
     * sans saturation donc sans défaut), ou pris pour la température ADC.
     * La conversion suivante redonnera un front.
     */
    if (gpio_get(Board::Rp2040::ADC_DRDY))
        return;

    //#define PRINT_CONVERSION_TIME
    #ifdef PRINT_CONVERSION_TIME
        uint32_t time = micros();
    #endif

    sequencer.onConversion(adc.readADC());

    #ifdef PRINT_CONVERSION_TIME
        Serial.println((micros()-time));
    #endif
}


double_t SensorBoard::getAdcTemperature() {

    return adcTemperature;
}

/**
 * Conversion from an ADC value to a resistance
 * 
 * This is where we try to compensate the T°C derivation of the whole measurement chain. (WIP)
 * 
 * @return double_t Résistance en Ohms
 */
double_t SensorBoard::computeResistance(Sensor& rtdSensor) {
    
    const uint8_t channel = channelFor(rtdSensor);

    // Une entrée thermocouple n'a pas de résistance exploitable.
    if (channel >= MAX_RTD ||
        rtdSensor.settings.type == Sensor::Type::Tc)
    {
        return NAN;
    }

    const CalibrationProfile& calibration = calibrationFor(rtdSensor.settings.type);

    const double_t gain = Sensor::measurementGain(rtdSensor.settings);

    /*
     * N0 est mesuré en 4 fils, au gain ADC de l'étendue précise. Le décalage
     * vient de l'entrée : en points ADC, il est proportionnel au gain ADC.
     */
    const double_t zeroOffset =
        settings.zeroOffset[channel] *
        Sensor::adcGain(rtdSensor.settings) / RTD_PRECISE_GAIN;

    const double_t correctedValue = rtdSensor.readValue() - zeroOffset;

    double_t Rrtd = correctedValue * calibration.refResistanceValue / (ADC_FULL_SCALE * gain);

    // Compensation de la mesure en fonction de la température
    // Je suis incapable de mesurer une différence notable sur la version  0.3
    //const double_t ppm = (adcTemperature - calibration.calTemperatureADC) * calibration.systemPPMCoeff;

    //Rrtd = Rrtd * (1 + ppm/1000000.0);

    // Valeur brute, éventuellement nulle ou négative : Resistance la classe
    // (court-circuit, hors étendue). NaN si l'acquisition n'a pas abouti.
    return Rrtd;
}

uint8_t SensorBoard::thermocoupleGain(Physics::Thermocouple::Type type)
{
    using Type = Physics::Thermocouple::Type;
    return type == Type::E || type == Type::J ? 16 : 32;
}

double_t SensorBoard::computeVoltage(const Sensor& sensor) const
{
    if (sensor.getBoard() != this || sensor.settings.type != Sensor::Type::Tc)
        return NAN;
    // Référence interne 2,048 V ; tension différentielle en mV.
    return sensor.readValue() * 2048.0 /
        (32768.0 * thermocoupleGain(sensor.settings.thermocoupleType));
}

double_t SensorBoard::getColdJunctionTemperature() const
{
    return adcTemperature + settings.coldJunctionOffset;
}

void SensorBoard::registerParameters(ParameterList& list)
{
    for (size_t i = 0; i < numSensors; i++)
    {
        rtd[i]->registerParameters(list);
    }

    auto parameters = list.forOwner(boardOwner(
        "sensor_board.thermocouple_calibration", "Thermocouples"));
    parameters.addDouble("cold_junction_offset", "C.J. offset",
        settings.coldJunctionOffset, -10, 10, 0.01, 2, "°C");

    auto readings = list.forOwner(boardOwner(
        "sensor_board.thermocouple_calibration", "Thermocouples", false));
    readings.addDouble("adc_temperature", "Temp. ADC",
        adcTemperature, "°C", true, 2);

    registerZeroCalibrationParameters(list);

    registerCalibrationParameters(
        list,
        Sensor::Type::Pt100);

    registerCalibrationParameters(
        list,
        Sensor::Type::Pt1000);
}

void SensorBoard::registerCalibrationParameters(
    ParameterList& list,
    Sensor::Type type)
{
    const bool isPt1000 =
        type == Sensor::Type::Pt1000;

    CalibrationProfile& calibration =
        calibrationFor(type);

    const double_t nominalReference =
        nominalReferenceResistance(type);

    const double_t nominalCalibrationResistance =
        isPt1000 ? 1000.0 : 100.0;

    auto parameters = list.forOwner(boardOwner(
        isPt1000
            ? "sensor_board.pt1000_calibration"
            : "sensor_board.pt100_calibration",
        isPt1000 ? "PT1000" : "PT100"));

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
    auto parameters = list.forOwner(boardOwner(
        "sensor_board.zero_calibration",
        "Zeros ADC"));

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
            Sensor::Type::Pt100,
            CALIBRATION_CHANNEL);
    }
    else if (actionId == CALIBRATE_PT1000_REFERENCE)
    {
        success = calibrateReference(
            Sensor::Type::Pt1000,
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
    Sensor::Type type)
{
    return
        type == Sensor::Type::Pt1000
            ? settings.pt1000
            : settings.pt100;
}

const SensorBoard::CalibrationProfile&
SensorBoard::calibrationFor(
    Sensor::Type type) const
{
    return
        type == Sensor::Type::Pt1000
            ? settings.pt1000
            : settings.pt100;
}

uint8_t SensorBoard::channelFor(
    const Sensor& sensor) const
{
    for (uint8_t channel = 0;
         channel < numSensors;
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
            Sensor::Type::Pt100,
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
    Sensor::Type type,
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
        RTD_PRECISE_GAIN /
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
    Sensor::Type type,
    uint8_t channel,
    CalibrationSamples& samples)
{
    if (channel >= MAX_RTD)
        return false;

    stopAcquisition();

    // Étendue précise par défaut : N0 et Rref sont pris au gain de référence.
    Sensor::Settings calibrationSettings{
        type,
        Sensor::Wiring::FourWire,
        0.0,
        CALIBRATION_SAMPLES
    };

    routeInput(calibrationSettings, channel);

    int32_t value = 0;

    for (uint16_t i = 0;
         i < CALIBRATION_DISCARDED_SAMPLES;
         i++)
    {
        if (!adc.readADC_Single(
                value,
                CALIBRATION_SAMPLE_TIMEOUT_MS))
        {
            stopCalibrationHardware();
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

    for (uint16_t i = 0;
         i < CALIBRATION_SAMPLES;
         i++)
    {
        if (!adc.readADC_Single(
                value,
                CALIBRATION_SAMPLE_TIMEOUT_MS))
        {
            stopCalibrationHardware();
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

    stopCalibrationHardware();

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

void SensorBoard::stopCalibrationHardware()
{
    mux.write(AnalogRoute{});
    adc.setIDACcurrent(CURRENT_0_UA);
    adc.setIDAC1routing(IDAC_DISABLED);
    adc.setIDAC2routing(IDAC_DISABLED);
}

double_t SensorBoard::nominalReferenceResistance(
    Sensor::Type type)
{
    return
        type == Sensor::Type::Pt1000
            ? 16500.0
            : 1650.0;
}

