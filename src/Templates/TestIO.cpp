#include <Templates/TestIO.h>

#include <Adafruit_GFX.h>
#include <Hardware/SensorBoard.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <hmi/HomeScreen.h>

namespace
{
    constexpr uint16_t BLACK = 0x0000;
    constexpr uint16_t WHITE = 0xFFFF;
    constexpr uint16_t GREEN = 0x07E0;
    constexpr uint16_t GREY = 0x8410;

    void printState(
        Adafruit_GFX& display, int16_t y,
        const char* label, bool valid, bool active,
        const char* onText = "ON  ")
    {
        display.setCursor(12, y);
        display.setTextColor(WHITE, BLACK);
        display.print(label);
        display.setCursor(144, y);
        display.setTextColor(valid && active ? GREEN : GREY, BLACK);
        display.print(!valid ? "--  " : active ? onText : "OFF ");
    }
}

const char* TestIO::name() const
{
    return "Test IO";
}

const char* TestIO::configurationKey() const
{
    return "test_io";
}

void TestIO::InputCommand::update(uint32_t now)
{
    (void)now;

    if (input == nullptr || !input->isValid())
    {
        invalidateCommand();
        return;
    }

    // 0,5 donne 50 % au PWM et ON au relais (seuil de ActuatorOnOff).
    writeCommand(input->isActive() ? 0.5 : 0.0);
}

bool TestIO::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    // Première sonde enregistrée : entrée analogique 1.
    pt100Input.begin(
        "test_io_pt100", "PT100",
        Sensor::Type::Pt100, Sensor::Wiring::FourWire, 16, 0.0f);

    if (!board.addSensor(pt100Input))
        return false;

    pt100Resistance.begin("Resistance PT100", board, pt100Input);
    pt100Temperature.begin("Temperature PT100", pt100Resistance);

    if (!process.add(pt100Resistance) || !process.add(pt100Temperature))
        return false;

    const uint8_t inputPins[] = {
        Board::Rp2040::DIGITAL_INPUT_1, Board::Rp2040::DIGITAL_INPUT_2
    };
    const uint8_t relayPins[] = {
        Board::Rp2040::OUTPUT_1, Board::Rp2040::OUTPUT_2
    };
    const uint8_t pwmPins[] = {
        Board::Rp2040::OUTPUT_3, Board::Rp2040::OUTPUT_4
    };
    const char* inputNames[] = {"Entree 1", "Entree 2"};
    const char* relayNames[] = {"Relais 1", "Relais 2"};
    const char* pwmNames[] = {"PWM 1", "PWM 2"};

    PWMOutput::sharedSettings.frequency = 20000;

    for (uint8_t i = 0; i < 2; i++)
    {
        inputs[i].begin(inputNames[i], inputPins[i]);
        commands[i].begin(inputNames[i]);
        commands[i].input = &inputs[i];
        relayActuators[i].begin(relayNames[i], commands[i]);
        pwmActuators[i].begin(pwmNames[i], commands[i]);
        relays[i].begin(relayNames[i], relayPins[i]);
        pwms[i].begin(pwmNames[i], pwmPins[i]);

        if (!process.add(inputs[i]) || !process.add(commands[i]) ||
            !process.add(relayActuators[i]) || !process.add(pwmActuators[i]) ||
            !process.connect(relayActuators[i], relays[i]) ||
            !process.connect(pwmActuators[i], pwms[i]))
        {
            return false;
        }
    }

    process.registerParameters(parameterList);

    // Essai reproductible : brochage, polarités et fréquence restent fixes.
    for (size_t i = 0; i < parameterList.count(); i++)
    {
        Parameter* parameter = parameterList.get(i);
        parameter->readOnly = true;
        parameter->persistent = false;
    }

    return !parameterList.hasError();
}

void TestIO::printHomeScreen(HomeScreenContext& context)
{
    Adafruit_GFX& display = context.display;
    display.setTextWrap(false);
    display.setTextSize(2);

    if (context.fullRefresh)
    {
        display.fillScreen(BLACK);
        display.setTextColor(WHITE, BLACK);
        display.setCursor(12, 12);
        display.print("Test IO");
        display.setTextSize(1);
        display.setCursor(12, 38);
        display.print("PWM : 20 kHz / 50 %");
        display.setCursor(12, 222);
        display.print("Etats commandes, sans retour contact");
        display.setTextSize(2);
    }

    for (uint8_t i = 0; i < 2; i++)
    {
        const int16_t y = 60 + i * 80;
        const DigitalInputSample* input = context.snapshot.find(inputs[i]);
        const OutputSample* relay = context.snapshot.find(relays[i]);
        const OutputSample* pwm = context.snapshot.find(pwms[i]);

        printState(display, y, inputs[i].getName(),
            input != nullptr && input->valid, input != nullptr && input->active);
        printState(display, y + 24, relays[i].getName(),
            relay != nullptr && relay->healthy,
            relay != nullptr && relay->appliedCommand >= 0.5);
        printState(display, y + 48, pwms[i].getName(),
            pwm != nullptr && pwm->healthy,
            pwm != nullptr && pwm->appliedCommand > 0.0, "50% ");
    }
}
