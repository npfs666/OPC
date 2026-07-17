#include <Measurements/Humidity/HumidityBME.h>

HumidityBME::HumidityBME(const char* name,
                         Adafruit_BME280& bme)
    : Humidity(name),
      m_bme(bme)
{
}

void HumidityBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(m_bme.readHumidity());

    setValid(true);
}