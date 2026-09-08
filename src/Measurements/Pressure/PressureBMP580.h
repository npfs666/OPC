#ifndef PRESSUREBMP580_H
#define PRESSUREBMP580_H

#include <Measurements/Pressure/Pressure.h>

class Adafruit_BMP5xx;

class PressureBMP580 : public Pressure
{
public:
    PressureBMP580();

    void begin(
        const char* name,
        Adafruit_BMP5xx& bmp580);

    double_t pressureSeaLevel(int16_t altitude);

    void update() override;

    double_t printValue() const override;

    uint8_t printDecimals() const override;

private:
    Adafruit_BMP5xx* bmp580 = nullptr;
};

#endif