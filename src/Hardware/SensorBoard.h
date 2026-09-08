#ifndef SENSORBOARD_h
#define SENSORBOARD_h

#include<Arduino.h>
#include<Hardware/pinout.h>
#include<Hardware/Sensor.h>
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
            1650.541,
            7.5,
            100.061,
            26.0
        };

        CalibrationProfile pt1000 = {
            16500.0,
            7.5,
            1000.0,
            25.0
        };

        double_t coldJunctionOffset = 0.0;

        double_t zeroOffset[MAX_RTD] = {};
    };

    Settings settings;

    ADS1120   adc;  // ADC
    AnalogMux mux;  // Front end multiplexers
    Sensor* rtd[MAX_RTD]; // Sensor array
    
    volatile bool newMeasurement=false;    // ADC has finished accumulating values, data is readable
    
    SensorBoard();
    void init();
    bool addSensor(Sensor& sensor);
    void startContinuous();
    void pause();
    void restart();
    void invert3WireIDAC();
    void adcInterrupt();
    void setStandardRTD();
    void setStandardTC();
    double_t getAdcTemperature();
    void setWiringRoute(Sensor::Settings settings);

    double_t getColdJunctionTemperature() const;
    double_t computeVoltage(const Sensor& sensor) const;

    double_t computeResistance(Sensor& rtdSensor);

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
        Sensor::Type type);
    const CalibrationProfile& calibrationFor(
        Sensor::Type type) const;

    uint8_t channelFor(
        const Sensor& sensor) const;

    bool calibrateZero(uint8_t channel);
    bool resetZeroOffsets();
    bool calibrateReference(
        Sensor::Type type,
        uint8_t channel);
    bool readCalibrationSamples(
        Sensor::Type type,
        uint8_t channel,
        CalibrationSamples& samples);
    void registerCalibrationParameters(
        ParameterList& list,
        Sensor::Type type);
    void registerZeroCalibrationParameters(
        ParameterList& list);
    void stopCalibrationHardware(
        uint8_t channel);

    static uint8_t thermocoupleGain(Physics::Thermocouple::Type type);

    static double_t nominalReferenceResistance(
        Sensor::Type type);
    static double_t measurementGain(
        Sensor::Wiring wiring);

    uint8_t curSensor;   // cur sensor index
    uint8_t numSensors;  // number of RTD sensors in rtd array[]
    volatile bool pauseInterrupts = true;
    volatile bool discardNextConversion = false;
    double_t adcTemperature;
    Settings calibrationBackup;
    bool calibrationBackupValid = false;
  };

#endif
