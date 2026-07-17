#include <Measurements/Humidity/HumidityPsychrometer.h>

PsychrometerHumidity::PsychrometerHumidity(
    const char* name,
    const Psychrometer& psychrometer)
    : Humidity(name),
      m_psychrometer(psychrometer)
{
}

void PsychrometerHumidity::update()
{
    setValue(m_psychrometer.relativeHumidity());

    setValid(true);
}