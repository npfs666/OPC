#include <Regulator/PID.h>

PID::PID(
    const char* name,
    Measurement& measurement)
    : Regulator(name),
      measurement(measurement)
{
    settings.setpoint  = 0.0;

    settings.kp = 1.0;
    settings.ki = 0.0;
    settings.kd = 0.0;

    settings.outputMin = 0.0;
    settings.outputMax = 1.0;

    reset();
}

void PID::reset()
{
    integral = 0.0;

    previousMeasurement = 0.0;

    previousTime = 0;

    initialized = false;

    writeCommand(0.0);
}

void PID::update(uint32_t now)
{
    double_t pv = measurement.getValue();

    if(!initialized)
    {
        previousMeasurement = pv;
        previousTime = now;
        initialized = true;
        return;
    }

    double_t dt = (double_t)(now - previousTime) / 1000.0;

    if(dt <= 0.0)
        return;

    double_t error = settings.setpoint - pv;

    integral += error * dt;

    double_t derivative =
        -(pv - previousMeasurement) / dt;

    double_t output =
        settings.kp * error +
        settings.ki * integral +
        settings.kd * derivative;

    output = constrain(
        output,
        settings.outputMin,
        settings.outputMax);

    writeCommand(output);

    previousMeasurement = pv;

    previousTime = now;
}