#include <Measurements/Humidity/HumidityBME.h>

HumidityBME::HumidityBME()
{
}

void HumidityBME::begin(const char* name, Adafruit_BME280& bme)
{
    Humidity::begin(name);
    this->bme = &bme;
}

void HumidityBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(bme->readHumidity());

    setValid(true);
}