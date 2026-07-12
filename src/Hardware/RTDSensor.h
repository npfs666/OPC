#ifndef RTDSENSOR_H
#define RTDSENSOR_H

#include <Arduino.h>

class RTDSensor
{

#define MAX_RTD 3
#define ALPHA 0.6f

public:
    enum class RTDType : uint8_t
    {
        Pt100,
        Pt1000
    };

    enum class RTDWiring : uint8_t
    {
        Wire2,
        Wire3,
        Wire4
    };

    struct Settings
    {
        RTDType type;

        RTDWiring wiring;

        double offset;

        uint16_t samples;
    };

    // Settings that can be configured in the menu
    Settings settings;
    

    RTDSensor();
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