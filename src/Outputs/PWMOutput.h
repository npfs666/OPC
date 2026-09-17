#ifndef PWMOUTPUT_H
#define PWMOUTPUT_H

#include <Outputs/Output.h>

class PWMOutput : public Output
{
public:
    struct Settings
    {
        uint8_t pin = 0;
        bool activeHigh = true;
        double_t safeCommand = 0.0;
    };

    struct SharedSettings
    {
        // OUTPUT_3 et OUTPUT_4 partagent le même compteur PWM.
        uint32_t frequency = 1000;
    };

    Settings settings;
    static SharedSettings sharedSettings;

    void begin(
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        double_t safeCommand = 0.0);

    void begin(
        const char* key,
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        double_t safeCommand = 0.0);

    bool begin() override;
    void poll(uint32_t now) override;
    void forceSafe() override;
    bool applySettings() override;
    bool isHealthy() const override;

    void registerParameters(ParameterList& list) override;
    bool validateParameters(const ParameterEditor& editor) const override;

private:
    void applyCommand(double_t command);
    static void writePhysicalCommand(
        uint8_t pin,
        double_t command,
        bool activeHigh);

    bool initialized = false;
    uint8_t configuredPin = 0;
    bool configuredActiveHigh = true;
    double_t configuredSafeCommand = 0.0;
};

#endif
