#pragma once

#include <Installation.h>
#include <Hardware/Sensor.h>
#include <Inputs/DigitalInput.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Outputs/ActuatorOnOff.h>
#include <Outputs/ActuatorPWM.h>
#include <Outputs/PWMOutput.h>
#include <Outputs/RelayOutput.h>
#include <Regulator/Regulator.h>

// Test matériel : entrée N -> relais N et PWM N à 50 %, 20 kHz.
class TestIO final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void printHomeScreen(HomeScreenContext& context) override;

private:
    class InputCommand : public Regulator
    {
    public:
        const DigitalInput* input = nullptr;

        void update(uint32_t now) override;
    };

    Sensor pt100Input;
    Resistance pt100Resistance;
    TemperatureRTD pt100Temperature;

    DigitalInput inputs[2];
    InputCommand commands[2];
    ActuatorOnOff relayActuators[2];
    ActuatorPWM pwmActuators[2];
    RelayOutput relays[2];
    PWMOutput pwms[2];
};
