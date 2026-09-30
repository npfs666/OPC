#include <Measurements/Temperature.h>

#include <Hardware/Sensor.h>

#include <Physics/PT100.h>
//#include <Measurement/PT1000.h>

Temperature::Temperature(const char* name,
                         Resistance& resistance)
    : Measurement(name, "°C"),
      m_resistance(resistance)
{
}

void Temperature::update()
{
    if(!m_resistance.valid())
    {
        setValid(false);
        return;
    }

    double temperature = 0.0;

    switch(m_resistance.sensor().settings.type)
    {
        case Sensor::Type::Pt100:
            temperature = PT100::getResistanceToTemperature(
                m_resistance.value());
            break;

        /*case Sensor::Type::Pt1000:
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