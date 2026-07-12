#include <Measurements/Pressure/PressureBME.h>

PressureBME::PressureBME(const char* name,
                         Adafruit_BME280& bme)
    : Pressure(name),
      m_bme(bme)
{
}

void PressureBME::update()
{
    //m_bme.takeForcedMeasurement();

    setValue(m_bme.readPressure());

    setValid(true);
}