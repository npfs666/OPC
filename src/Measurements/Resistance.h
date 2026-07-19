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

    Resistance(const char* name,
               SensorBoard& board,
               RTDSensor& sensor);

    virtual ~Resistance() = default;

    void update() override;

    RTDSensor& getSensor();

    const RTDSensor& getSensor() const;

private:

    SensorBoard& board;
    RTDSensor&   sensor;
};

#endif