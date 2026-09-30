#include <Measurements/Temperature.h>
#include <Physics/PT100.h>

void TemperatureMeasurement::update()
{
    double_t resistance = board.getResistanceValue(sensorIndex);

    _value = PT100::getResistanceToTemperature(resistance);

}