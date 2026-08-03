#ifndef SENSORBOARD_h
#define SENSORBOARD_h

#include<Arduino.h>
#include<Hardware/pinout.h>
#include<Hardware/RTDSensor.h>
#include<Drivers/ADS1120.h>
#include<Hardware/AnalogMux.h>
#include <Configurable.h>

class SensorBoard: public Configurable {

public:

    struct Settings
    {
        double_t refResistanceValue   = 1649.819;   // Value of Rref after calibration (Ohm)
        double_t systemPPMCoeff       = 7.5;        // Coefficient of system °C/K
        double_t calResistanceValue   = 100.056;    // Value of calibration resistance used.
        double_t calTemperatureADC    = 22.12;      // ADC calibration temperature (°C)
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
    void calRefResistor();
    void invert3WireIDAC();
    void adcInterrupt();
    void setStandartRTD();
    void setWiringRoute(RTDSensor::Settings settings);

    double_t computeResistance(RTDSensor& rtdSensor);

    void registerParameters(ParameterList& list) override;
    void resetAcquisition();

private:

    uint8_t curRTDSensor;   // cur sensor index
    uint8_t numRTDSensors;  // number of RTD sensors in rtd array[]
    volatile bool pauseInterrupts = true;
    double_t adcTemperature;
  };

#endif