#include <Measurements/Resistance.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/RTDSensor.h>

Resistance::Resistance(const char* name,
                       SensorBoard& board,
                       RTDSensor& sensor)
    : Measurement(name, "Ohm"),
      m_board(board),
      m_sensor(sensor)
{
}

void Resistance::update()
{
    /*Serial.print("ADC moyenne ");
    Serial.println(m_sensor.readValue());
    Serial.print("resistance ");
    Serial.println(value());*/
    setValue(m_board.computeResistance(m_sensor));
    setValid(true);
}

RTDSensor& Resistance::sensor()
{
    return m_sensor;
}

const RTDSensor& Resistance::sensor() const
{
    return m_sensor;
}