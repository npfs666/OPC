#include <Measurements/Measurement.h>
#include <Hardware/SensorBoard.h>

class TemperatureMeasurement : public Measurement
{
public:

    TemperatureMeasurement(
        SensorBoard& board,
        uint8_t sensor,
        const char* name);

    void update() override;

private:

    SensorBoard& board;

    uint8_t sensorIndex;
};