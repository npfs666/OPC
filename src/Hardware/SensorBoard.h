#ifndef SENSORBOARD_h
#define SENSORBOARD_h

#include<Arduino.h>
#include<Hardware/pinout.h>
#include<Hardware/RTDSensor.h>
#include<Drivers/ADS1120.h>
#include<Hardware/AnalogMux.h>
#include <Configurable.h>
#include <hmi/MenuBuilder.h>

class SensorBoard: public Configurable {

public:

    struct CalibrationProfile
    {
        double_t refResistanceValue;
        double_t systemPPMCoeff;
        double_t calResistanceValue;
        double_t calTemperatureADC;
    };

    struct Settings
    {
        CalibrationProfile pt100 = {
            1649.819,
            7.5,
            100.056,
            22.12
        };

        CalibrationProfile pt1000 = {
            16500.0,
            7.5,
            1000.0,
            25.0
        };

        double_t zeroOffset[MAX_RTD] = {};
    };

    Settings settings;

    ADS1120   adc;  // ADC
    AnalogMux mux;  // Front end multiplexers
    RTDSensor* rtd[MAX_RTD]; // Sensor array
    
    volatile bool newMeasurement=false;    // ADC has finished accumulating values, data is readable
    
    SensorBoard();
    void init();
    bool addRTD(RTDSensor& sensor);
    void startContinuous();
    void pause();
    void restart();
    void invert3WireIDAC();
    void adcInterrupt();
    void setStandartRTD();
    void setWiringRoute(RTDSensor::Settings settings);

    double_t computeResistance(RTDSensor& rtdSensor);

    void registerParameters(ParameterList& list) override;
    bool addMenuActions(MenuBuilder& menu) const;
    bool handlesMenuAction(
        MenuBuilder::ActionId actionId) const;
    bool executeMenuAction(
        MenuBuilder::ActionId actionId);
    void onMenuActionSaved(
        MenuBuilder::ActionId actionId);
    void onMenuActionSaveFailed(
        MenuBuilder::ActionId actionId);
    void resetAcquisition();

private:

    struct CalibrationSamples
    {
        double_t average = 0.0;
    };

    static constexpr MenuBuilder::ActionId
        CALIBRATE_ZERO_INPUT_1 = 32;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_ZERO_INPUT_2 = 33;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_ZERO_INPUT_3 = 34;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_PT100_REFERENCE = 35;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_PT1000_REFERENCE = 36;
    static constexpr MenuBuilder::ActionId
        RESET_ZERO_OFFSETS = 37;

    CalibrationProfile& calibrationFor(
        RTDSensor::RTDType type);
    const CalibrationProfile& calibrationFor(
        RTDSensor::RTDType type) const;

    uint8_t channelFor(
        const RTDSensor& sensor) const;

    bool calibrateZero(uint8_t channel);
    bool resetZeroOffsets();
    bool calibrateReference(
        RTDSensor::RTDType type,
        uint8_t channel);
    bool readCalibrationSamples(
        RTDSensor::RTDType type,
        uint8_t channel,
        CalibrationSamples& samples);
    void registerCalibrationParameters(
        ParameterList& list,
        RTDSensor::RTDType type);
    void registerZeroCalibrationParameters(
        ParameterList& list);
    void stopCalibrationHardware(
        uint8_t channel);

    static double_t nominalReferenceResistance(
        RTDSensor::RTDType type);
    static double_t measurementGain(
        RTDSensor::RTDWiring wiring);

    uint8_t curRTDSensor;   // cur sensor index
    uint8_t numRTDSensors;  // number of RTD sensors in rtd array[]
    volatile bool pauseInterrupts = true;
    volatile bool discardNextConversion = false;
    double_t adcTemperature;
    Settings calibrationBackup;
    bool calibrationBackupValid = false;
  };

#endif
