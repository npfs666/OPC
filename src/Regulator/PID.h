#ifndef PID_H
#define PID_H

#include <Regulator/Regulator.h>

class Measurement;

class PID : public Regulator
{
public:

    struct Settings
    {
        double_t setpoint;

        double_t kp;
        double_t ki;
        double_t kd;

        double_t outputMin;
        double_t outputMax;
    };

    Settings settings;

    PID();

    void begin(const char* name,
        Measurement& measurement);

    void begin(
        const char* key,
        const char* name,
        Measurement& measurement);

    void reset();

    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    void registerParameters(ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor)
        const override;
    
    void print(Stream& stream) const override;

private:

    Measurement* measurement = nullptr;

    double_t integral = 0.0;

    double_t previousMeasurement = 0.0;

    uint32_t previousTime = 0;

    bool initialized = false;
};

#endif
