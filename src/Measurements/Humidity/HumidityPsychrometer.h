#ifndef HUMIDITYPSYCHROMETER_H
#define HUMIDITYPSYCHROMETER_H

#include <Measurements/Humidity/Humidity.h>

class Psychrometer;

class HumidityPsychrometer : public Humidity
{
public:
    HumidityPsychrometer();

    void begin(const char* name, const Psychrometer& psychrometer);

    void update() override;

private:

    const Psychrometer* psychrometer = nullptr;
};

#endif
