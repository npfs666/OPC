#include <Measurements/Temperature/TemperatureBME.h>
#include <Measurements/Measurement.h>

TemperatureBME::TemperatureBME(const char* name,
                         Adafruit_BME280& bme)
    : Temperature(name),
      m_bme(bme)
{
}

void TemperatureBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(m_bme.readTemperature());

    setValid(true);
}