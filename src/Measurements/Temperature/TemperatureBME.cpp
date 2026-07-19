#include <Measurements/Temperature/TemperatureBME.h>
#include <Measurements/Measurement.h>

TemperatureBME::TemperatureBME(const char* name,
                         Adafruit_BME280& bme)
    : Temperature(name),
      bme(bme)
{
}

void TemperatureBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(bme.readTemperature());

    setValid(true);
}