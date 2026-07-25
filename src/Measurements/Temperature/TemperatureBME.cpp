#include <Measurements/Temperature/TemperatureBME.h>
#include <Measurements/Measurement.h>

TemperatureBME::TemperatureBME()
{
}


void TemperatureBME::begin(const char* name, Adafruit_BME280& bme)
{
    Temperature::begin(name);
    this->bme = &bme;
}

void TemperatureBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(bme->readTemperature());

    setValid(true);
}