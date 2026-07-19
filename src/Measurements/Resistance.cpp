#include <Measurements/Resistance.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/RTDSensor.h>

Resistance::Resistance(const char* name,
                       SensorBoard& board,
                       RTDSensor& sensor)
    : Measurement(name, "Ohm"),
      board(board),
      sensor(sensor)
{
}

void Resistance::update()
{
    /*Serial.print("ADC moyenne ");
    Serial.println(m_sensor.readValue());
    Serial.print("resistance ");
    Serial.println(value());*/
    setValue(board.computeResistance(sensor));
    setValid(true);
}

RTDSensor& Resistance::getSensor()
{
    return sensor;
}

const RTDSensor& Resistance::getSensor() const
{
    return sensor;
}