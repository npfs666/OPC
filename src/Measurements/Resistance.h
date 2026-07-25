#ifndef RESISTANCE_H
#define RESISTANCE_H

#include <Measurements/Measurement.h>

class SensorBoard;
class RTDSensor;

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
               RTDSensor& sensor);

    void update() override;

    RTDSensor& getSensor();

    const RTDSensor& getSensor() const;

    uint8_t printDecimals() const override;

private:

    SensorBoard* board = nullptr;
    RTDSensor*   sensor = nullptr;
};

#endif