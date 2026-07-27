#include <Regulator/PID.h>

PID::PID()
{
}

void PID::begin(
    const char* name,
    Measurement& measurement)
{
    begin(name, name, measurement);
}

void PID::begin(
    const char* key,
    const char* name,
    Measurement& measurement)
{
    Regulator::begin(key, name);

    this->measurement = &measurement;

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
    double_t pv = measurement->getValue();

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

void PID::registerParameters(ParameterList& list) {

    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getKey(),
        getName()
    });

    parameters.addDouble(
        "setpoint",
        "Consigne",
        settings.setpoint,
        0.0,
        80.0,
        0.5,
        1,
        "°C");

    parameters.addDouble(
        "kp",
        "Kp",
        settings.kp,
        0.0,
        10.0,
        0.01,
        2,
        "1/°C");

    parameters.addDouble(
        "ki",
        "Ki",
        settings.ki,
        0.0,
        0.1,
        0.001,
        3,
        "1/(°C.s)");

    parameters.addDouble(
        "kd",
        "Kd",
        settings.kd,
        0.0,
        100.0,
        0.1,
        1,
        "s/°C");

    parameters.addDouble(
        "output_min",
        "Sortie min",
        settings.outputMin,
        0.0,
        1.0,
        0.01,
        2);

    parameters.addDouble(
        "output_max",
        "Sortie max",
        settings.outputMax,
        0.0,
        1.0,
        0.01,
        2);
}
