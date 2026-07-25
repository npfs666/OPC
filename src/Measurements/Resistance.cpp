#include <Measurements/Resistance.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/RTDSensor.h>

Resistance::Resistance()
{
}

void Resistance::begin(const char* name,
                       SensorBoard& board,
                       RTDSensor& sensor)
{
    Measurement::begin(name, "Ω");

    this->board = &board;
    this->sensor = &sensor;
}

void Resistance::update()
{
    /*Serial.print("ADC moyenne ");
    Serial.println(m_sensor.readValue());
    Serial.print("resistance ");
    Serial.println(value());*/
    setValue(board->computeResistance(*sensor));
    setValid(true);
}

RTDSensor& Resistance::getSensor()
{
    return *sensor;
}

const RTDSensor& Resistance::getSensor() const
{
    return *sensor;
}

uint8_t Resistance::printDecimals() const
{
    return 3;
}