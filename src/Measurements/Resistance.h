#ifndef RESISTANCE_H
#define RESISTANCE_H

#include <Measurements/Measurement.h>

class SensorBoard;
class Sensor;

/**
 * @brief Représente une mesure de résistance (Ohms).
 *
 * La conversion ADC -> Ohms est réalisée par SensorBoard.
 * Cette classe expose simplement cette grandeur physique
 * au reste de l'application.
 */
class Resistance : public Measurement
{
public:

    Resistance();

    virtual ~Resistance() = default;

    void begin(const char* name,
               SensorBoard& board,
               Sensor& sensor);

    void update() override;

    Sensor& getSensor();

    const Sensor& getSensor() const;

    uint8_t printDecimals() const override;

private:

    SensorBoard* board = nullptr;
    Sensor*   sensor = nullptr;
};

#endif