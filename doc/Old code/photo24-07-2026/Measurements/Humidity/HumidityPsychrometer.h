#ifndef HUMIDITYPSYCHROMETER_H
#define HUMIDITYPSYCHROMETER_H

#include <Measurements/Humidity/Humidity.h>
#include <Measurements/Psychrometer.h>

class HumidityPsychrometer : public Humidity
{
public:

    HumidityPsychrometer(const char* name,
                         const Psychrometer& psychrometer);

    void update() override;

private:

    const Psychrometer& psychrometer;
};

#endif