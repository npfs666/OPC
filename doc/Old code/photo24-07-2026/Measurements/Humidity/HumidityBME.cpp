#include <Measurements/Humidity/HumidityBME.h>

HumidityBME::HumidityBME(const char* name,
                         Adafruit_BME280& bme)
    : Humidity(name),
      bme(bme)
{
}

void HumidityBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(bme.readHumidity());

    setValid(true);
}