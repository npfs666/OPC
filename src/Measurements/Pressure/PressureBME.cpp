#include <Measurements/Pressure/PressureBME.h>

PressureBME::PressureBME(const char* name,
                         Adafruit_BME280& bme)
    : Pressure(name),
      bme(bme)
{
}

void PressureBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(bme.readPressure());

    setValid(true);
}