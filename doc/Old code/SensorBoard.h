#ifndef SENSORBOARD_H
#define SENSORBOARD_H

#include<Arduino.h>
#include<Hardware/Sensor.h>
#include<Drivers/ADS1120.h>
#include<Hardware/AnalogMux.h>

class SensorBoard
{
public:
    SensorBoard();
    void begin();

    //void addSensor(Sensor* sensor);
    void addSensor(uint8_t type, uint8_t switchPin, uint16_t samples, float_t offset);
    void enableCurrentSensor();
    void disableCurrentSensor();
    void configureCurrentSensor();
    void finishCurrentSensor();
    void sampleReady(int32_t value);
    void onDataReady();
    double getRTDResistance(uint8_t index);
    double getRTDTemperature(uint8_t index);
    uint8_t sensorCount() const;
    void set4WirePT100();
    void set3WirePT100();
    void invert3WireIDAC();
    void start();
    void stop();
    void restart();
    void startContinuous();

    bool available() const;

    void update();

    //Sensor& sensor(uint8_t);
    bool newMeasurement;    // ADC has finished accumulating values, data is readable

private:

    ADS1120 adc;

    AnalogMux mux;

    Sensor rtd[3]; // Sensor array

    uint8_t currentSensor;

    uint8_t numSensors;  // number of RTD sensors
    uint8_t curSensor;   // cur sensor index
    double temperatureADC;
    
};

#endif