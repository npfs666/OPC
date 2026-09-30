#include <Measurements/Resistance.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/Sensor.h>

Resistance::Resistance(const char* name,
                       SensorBoard& board,
                       Sensor& sensor)
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

Sensor& Resistance::sensor()
{
    return m_sensor;
}

const Sensor& Resistance::sensor() const
{
    return m_sensor;
}