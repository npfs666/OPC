#include <Outputs/PWMOutput.h>

#include <Arduino.h>
#include <Hardware/pinout.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    // Résolution fixe de 0,1 %, y compris lorsque la fréquence change.
    constexpr uint16_t PWM_RANGE = 1000;
    constexpr uint32_t MIN_FREQUENCY = 1000;
    constexpr uint32_t MAX_FREQUENCY = 100000;

    constexpr ParameterOption PWM_PIN_OPTIONS[] = {
        {Board::Rp2040::OUTPUT_3, "PWM 1"},
        {Board::Rp2040::OUTPUT_4, "PWM 2"}
    };

    bool isPWMPin(uint8_t pin)
    {
        return pin == Board::Rp2040::OUTPUT_3 ||
               pin == Board::Rp2040::OUTPUT_4;
    }
}

PWMOutput::SharedSettings PWMOutput::sharedSettings;

void PWMOutput::begin(
    const char* name,
    uint8_t pin,
    bool activeHigh,
    double_t safeCommand)
{
    begin(name, name, pin, activeHigh, safeCommand);
}

void PWMOutput::begin(
    const char* key,
    const char* name,
    uint8_t pin,
    bool activeHigh,
    double_t safeCommand)
{
    Output::begin(key, name);
    settings.pin = pin;
    settings.activeHigh = activeHigh;
    settings.safeCommand = safeCommand;
}

bool PWMOutput::begin()
{
    if (initialized)
        forceSafe();

    initialized = false;

    if (!isPWMPin(settings.pin) ||
        !std::isfinite(settings.safeCommand) ||
        settings.safeCommand < 0.0 ||
        settings.safeCommand > 1.0 ||
        sharedSettings.frequency < MIN_FREQUENCY ||
        sharedSettings.frequency > MAX_FREQUENCY)
    {
        return false;
    }

    const float divider =
        static_cast<float>(clock_get_hz(clk_sys)) /
        (PWM_RANGE * sharedSettings.frequency);

    if (divider < 1.0f || divider >= 256.0f)
        return false;

    configuredPin = settings.pin;
    configuredActiveHigh = settings.activeHigh;
    configuredSafeCommand = settings.safeCommand;

    // Ne pas utiliser pwm_init : il effacerait aussi le rapport cyclique
    // de l'autre canal du compteur partagé.
    const uint slice = pwm_gpio_to_slice_num(configuredPin);
    pwm_set_clkdiv(slice, divider);
    pwm_set_wrap(slice, PWM_RANGE - 1);
    pwm_set_enabled(slice, true);

    initialized = true;
    forceSafe();
    return true;
}

void PWMOutput::poll(uint32_t now)
{
    (void)now;

    if (initialized && requestedCommand() != appliedCommand())
        applyCommand(requestedCommand());
}

void PWMOutput::forceSafe()
{
    requested = configuredSafeCommand;

    if (initialized)
        applyCommand(configuredSafeCommand);
    else
        setAppliedCommand(configuredSafeCommand);
}

bool PWMOutput::applySettings()
{
    return begin();
}

bool PWMOutput::isHealthy() const
{
    return initialized;
}

void PWMOutput::applyCommand(double_t command)
{
    writePhysicalCommand(configuredPin, command, configuredActiveHigh);
    setAppliedCommand(command);
}

void PWMOutput::writePhysicalCommand(
    uint8_t pin,
    double_t command,
    bool activeHigh)
{
    const double_t physicalCommand = activeHigh ? command : 1.0 - command;
    const uint16_t level = static_cast<uint16_t>(
        std::lround(physicalCommand * PWM_RANGE));

    if (level == 0 || level == PWM_RANGE)
    {
        // Niveaux constants à 0/100 %, avec repli immédiat hors du PWM.
        gpio_put(pin, level != 0);
        gpio_set_dir(pin, true);
        gpio_set_function(pin, GPIO_FUNC_SIO);
        return;
    }

    pwm_set_gpio_level(pin, level);
    gpio_set_function(pin, GPIO_FUNC_PWM);
}

void PWMOutput::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "outputs", "Sorties", getConfigurationKey(), getName()
    });

    parameters.addSelection("pin", "Broche", settings.pin, PWM_PIN_OPTIONS);
    parameters.addBool("active_high", "Actif à HIGH", settings.activeHigh);
    parameters.addDouble(
        "safe_command", "Commande de sécurité", settings.safeCommand,
        0.0, 1.0, 0.01, 2);

    // Une seule entrée persistante pour la fréquence des deux canaux.
    if (list.find("pwm", "frequency") == nullptr)
    {
        auto common = list.forOwner({
            "outputs", "Sorties", "pwm", "PWM commun"
        });
        common.addInteger(
            "frequency", "Fréquence", sharedSettings.frequency,
            MIN_FREQUENCY, MAX_FREQUENCY, 100, "Hz");
    }
}

bool PWMOutput::validateParameters(const ParameterEditor& editor) const
{
    const ParameterDraft* pin = editor.find(getConfigurationKey(), "pin");

    if (pin == nullptr)
        return false;

    // Deux sorties ne doivent pas piloter la même broche après édition.
    for (size_t i = 0; i < editor.count(); i++)
    {
        const ParameterDraft& other = editor.get(i);
        const Parameter* parameter = other.parameter;

        if (&other != pin && parameter != nullptr &&
            parameter->type == Parameter::Type::Selection &&
            std::strcmp(parameter->categoryKey, "outputs") == 0 &&
            std::strcmp(parameter->key, "pin") == 0 &&
            other.selectionValue == pin->selectionValue)
        {
            return false;
        }
    }

    return true;
}
