#ifndef SENSORBOARD_h
#define SENSORBOARD_h

#include<Arduino.h>
#include<Hardware/pinout.h>
#include<Hardware/Sensor.h>
#include<Drivers/ADS1120.h>
#include<Hardware/AnalogMux.h>

class SensorBoard {

public:

    struct Settings
    {
        double_t refResistanceValue   = 1649.819; // Value of Rref after calibration
        double_t refResistanceCoeff   = 0;        // Coefficient of Rref °C/K (maybe measure the whole system to be more precise)
        double_t calResistanceValue   = 100.056;  // Value of calibration resistance used.
        double_t calTemperatureADC    = 22.12;    // ADC calibration temperature (°C)
        double_t calTemperatureBME    = 23.12;    // BME280 calibration temperature (°C)
    };

    Settings settings;

    ADS1120   adc;  // ADC
    AnalogMux mux;  // Front end multiplexers
    Sensor rtd[MAX_RTD]; // Sensor array
    
    bool newMeasurement=false;    // ADC has finished accumulating values, data is readable
    
    SensorBoard();
    void init();
    void addSensor(Sensor::Type type, Sensor::Wiring wiring, uint16_t samples, float_t offset);
    void startContinuous();
    void pause();
    void restart();
    void calRefResistor();
    void invert3WireIDAC();
    void adcInterrupt();
    void setStandartRTD();
    void setWiringRoute(Sensor::Settings settings);

    double_t computeResistance(Sensor& rtdSensor);

private:

    uint8_t curSensor;   // cur sensor index
    uint8_t numSensors;  // number of RTD sensors in rtd array[]
  };

#endif