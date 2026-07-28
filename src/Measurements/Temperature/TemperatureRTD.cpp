#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Measurement.h>
#include <Hardware/RTDSensor.h>

#include <Physics/PT100.h>
#include <cmath>
//#include <Measurement/PT1000.h>

TemperatureRTD::TemperatureRTD()
{
}

void TemperatureRTD::begin(const char* name,
                         Resistance& resistance)
{
    Temperature::begin(name);
    this->resistance = &resistance;
}



void TemperatureRTD::update()
{
    if(!resistance->isValid())
    {
        setValid(false);
        return;
    }

    double temperature = 0.0;

    switch(resistance->getSensor().settings.type)
    {
        case RTDSensor::RTDType::Pt100:
            temperature = PT100::getResistanceToTemperature(
                resistance->getValue());
            break;

        /*case RTDSensor::RTDType::Pt1000:
            temperature = PT1000::resistanceToTemperature(
                m_resistance.value());
            break;*/

        default:
            setValid(false);
            return;
    }

    temperature += resistance->getSensor().settings.offset;

    if (!std::isfinite(temperature))
    {
        setValid(false);
        return;
    }

    setValue(temperature);
    setValid(true);
}
