#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Measurements/Measurement.h>
#include <Measurements/Resistance.h>

class Temperature : public Measurement
{
public:

    Temperature(const char* name,
                Resistance& resistance);

    virtual ~Temperature() = default;

    void update() override;

private:

    Resistance& m_resistance;
};

#endif