#ifndef HUMIDITYPSYCHROMETER_H
#define HUMIDITYPSYCHROMETER_H

#include <Measurements/Humidity/Humidity.h>
#include <Measurements/Psychrometer.h>

class PsychrometerHumidity : public Humidity
{
public:

    PsychrometerHumidity(const char* name,
                         const Psychrometer& psychrometer);

    void update() override;

private:

    const Psychrometer& m_psychrometer;
};

#endif