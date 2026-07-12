#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Measurement.h>
#include <Hardware/RTDSensor.h>

#include <Physics/PT100.h>
//#include <Measurement/PT1000.h>

TemperatureRTD::TemperatureRTD(const char* name,
                         Resistance& resistance)
    : Temperature(name),
      m_resistance(resistance)
{
}

void TemperatureRTD::update()
{
    if(!m_resistance.valid())
    {
        setValid(false);
        return;
    }

    double temperature = 0.0;

    switch(m_resistance.sensor().settings.type)
    {
        case RTDSensor::RTDType::Pt100:
            temperature = PT100::getResistanceToTemperature(
                m_resistance.value());
            break;

        /*case RTDSensor::RTDType::Pt1000:
            temperature = PT1000::resistanceToTemperature(
                m_resistance.value());
            break;*/

        default:
            setValid(false);
            return;
    }

    temperature += m_resistance.sensor().settings.offset;

    setValue(temperature);
    setValid(true);
}