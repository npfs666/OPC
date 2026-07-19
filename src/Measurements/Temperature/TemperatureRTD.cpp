#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Measurement.h>
#include <Hardware/RTDSensor.h>

#include <Physics/PT100.h>
//#include <Measurement/PT1000.h>

TemperatureRTD::TemperatureRTD(const char* name,
                         Resistance& resistance)
    : Temperature(name),
      resistance(resistance)
{
}

void TemperatureRTD::update()
{
    if(!resistance.isValid())
    {
        setValid(false);
        return;
    }

    double temperature = 0.0;

    switch(resistance.getSensor().settings.type)
    {
        case RTDSensor::RTDType::Pt100:
            temperature = PT100::getResistanceToTemperature(
                resistance.getValue());
            break;

        /*case RTDSensor::RTDType::Pt1000:
            temperature = PT1000::resistanceToTemperature(
                m_resistance.value());
            break;*/

        default:
            setValid(false);
            return;
    }

    temperature += resistance.getSensor().settings.offset;

    setValue(temperature);
    setValid(true);
}