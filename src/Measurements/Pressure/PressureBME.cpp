#include <Measurements/Pressure/PressureBME.h>
#include <math.h>

PressureBME::PressureBME(const char* name,
                         Adafruit_BME280& bme)
    : Pressure(name),
      bme(bme)
{
}

double_t PressureBME::pressureSeaLevel(int16_t altitude) {

    double_t pressure = (double_t)(bme.readPressure() / pow(1.0 - ((0.0065 * altitude) / (bme.readTemperature() + 273.14)), 5.255));

    return pressure;
}

double_t PressureBME::printValue() const {
    return getValue() / 100.0;
}

uint8_t PressureBME::printDecimals() const {
    return 1;
}

void PressureBME::update()
{
    //m_bme.takeForcedMeasurement();

    //setValue(pressureSeaLevel(27));
    setValue(bme.readPressure());

    setValid(true);
}