#ifndef HUMIDITY_PSYCHROMERIC_h
#define HUMIDITY_PSYCHROMERIC_h

#include <Measurements/Measurement.h>
#include <Hardware/SensorBoard.h>

class HumidityMeasurement : public Measurement
{
public:

    HumidityMeasurement(
        SensorBoard& board,
        uint8_t dry,
        uint8_t wet);

    void update() override;

private:

    SensorBoard& board;

    uint8_t drySensor;

    uint8_t wetSensor;
};

#endif