#ifndef RTDSENSOR_H
#define RTDSENSOR_H

#include <Arduino.h>
#include <Hardware/pinout.h>

class RTDSensor
{

public:
    enum class RTDType : uint8_t
    {
        Pt100,
        Pt1000
    };

    // Measurement method
    enum class RTDWiring : uint8_t
    {
        TwoWire,
        ThreeWire,
        FourWire
    };

    // Settings that can be configured in the menu and needs to be public
    struct Settings
    {
        RTDType type;
        RTDWiring wiring;
        double_t offset;
        uint16_t samples;
    };

    Settings settings;

    RTDSensor();
    
    /**
     * @brief Construct a new RTDSensor::RTDSensor object
     *
     * @param type 3 or 4 Wire type
     * @param samples 4 samples -> 1bit improve, 16 -> 2bits, 64 -> 3bits, 256 -> 4bits (oversampling)
     * @param offset Sensor offset in °C
     */
    RTDSensor(RTDType type, RTDWiring wiring, uint16_t samples, float_t offset);
    void reset();
    void add(int32_t value);
    void addLP(int32_t value);
    void compute();
    bool isAccumulationHalfWay();
    bool isAccumulationDone();
    double_t readValue() const;

private:
    double_t nMinusOneValue;
    double_t sum;
    double_t resistance;
    uint16_t sampleCount;
    double_t avgValue;
};

#endif