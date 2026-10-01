#include <Templates/testInstallation.h>

#include <Hardware/SensorBoard.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>

#include <hmi/HomeScreen.h>
#include <hmi/MeasurementDisplay.h>
#include <ProcessSnapshot.h>

namespace
{
    // Entrée 1 : false = PT100 4 fils, true = thermocouple type K.
    constexpr bool INPUT1_IS_THERMOCOUPLE = false;

    constexpr uint16_t SAMPLES = 16;

    // Écran d'accueil
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
    constexpr uint16_t COLOR_GREY = 0x8410;

    constexpr int16_t LABEL_X = 8;
    constexpr int16_t VALUE_X = 60;
    constexpr int16_t ROW_HEIGHT = 20;

    constexpr int16_t T1_Y = 38;
    constexpr int16_t T2_Y = 70;
    constexpr int16_t HR_Y = 102;

    void printLabel(
        Adafruit_GFX& display,
        int16_t y,
        const char* label)
    {
        display.setCursor(LABEL_X, y);
        display.print(label);
    }

    void printValue(
        Adafruit_GFX& display,
        int16_t y,
        const MeasurementSample* sample)
    {
        // Efface l'ancienne valeur sans toucher à l'étiquette.
        display.fillRect(
            VALUE_X - 5,
            y,
            display.width() - (VALUE_X - 5),
            ROW_HEIGHT,
            COLOR_BLACK);

        display.setCursor(VALUE_X, y);

        MeasurementDisplay::print(
            display,
            sample,
            COLOR_WHITE,
            COLOR_BLACK);
    }
}

const char* TestInstallation::name() const
{
    return "Installation de test pour le développement";
}

const char* TestInstallation::configurationKey() const
{
    return "test_installation";
}

bool TestInstallation::requiresBMP580() const
{
    return true;
}

const Temperature& TestInstallation::dryBulbTemperature() const
{
    if (INPUT1_IS_THERMOCOUPLE)
        return tcTemperature;

    return rtd1Temperature;
}

bool TestInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    // ----- Entrées physiques -----

    if (INPUT1_IS_THERMOCOUPLE)
        input1.begin("input1", "Input 1", Sensor::Type::Tc, Sensor::Wiring::TwoWire, SAMPLES, 0);
    else
        input1.begin("input1", "Input 1", Sensor::Type::Pt100, Sensor::Wiring::FourWire, SAMPLES, 0);

    input2.begin("input2", "Input 2", Sensor::Type::Pt100, Sensor::Wiring::FourWire, SAMPLES, 0);

    if (!board.addSensor(input1) || !board.addSensor(input2))
        return fail("Entrées analogiques indisponibles");

    // ----- Entrée 1 : température sèche -----

    if (INPUT1_IS_THERMOCOUPLE)
    {
        tcTemperature.begin("TempTC", input1);

        if (!process.add(tcTemperature))
            return fail("Mesure thermocouple non enregistrée");
    }
    else
    {
        rtd1Resistance.begin("RTD1", board, input1);
        rtd1Temperature.begin("TempRTD1", rtd1Resistance);

        if (!process.add(rtd1Resistance) || !process.add(rtd1Temperature))
            return fail("Mesures entrée 1 non enregistrées");
    }

    // ----- Entrée 2 : température humide -----

    rtd2Resistance.begin("RTD2", board, input2);
    rtd2Temperature.begin("TempRTD2", rtd2Resistance);

    if (!process.add(rtd2Resistance) || !process.add(rtd2Temperature))
        return fail("Mesures entrée 2 non enregistrées");

    // ----- Pression et humidité -----

    pressure.begin("BMP580", bmp580);
    psychrometer.begin(dryBulbTemperature(), rtd2Temperature, pressure);
    humidity.begin("RH psychrom", psychrometer);

    if (!process.add(pressure) || !process.add(humidity))
        return fail("Mesures pression / humidité non enregistrées");

    // ----- Paramètres du menu -----

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres invalides");

    return true;
}

void TestInstallation::printHomeScreen(
    HomeScreenContext& context)
{
    Adafruit_GFX& display = context.display;

    display.cp437(true);
    display.setTextWrap(false);
    display.setTextSize(2);

    if (context.fullRefresh)
    {
        display.fillScreen(COLOR_BLACK);

        display.setTextColor(COLOR_CYAN, COLOR_BLACK);
        display.setCursor(LABEL_X, 5);
        display.print("OPC - Accueil");

        display.drawFastHLine(0, 25, display.width(), COLOR_GREY);

        display.setTextColor(COLOR_WHITE, COLOR_BLACK);
        printLabel(display, T1_Y, "T1");
        printLabel(display, T2_Y, "T2");
        printLabel(display, HR_Y, "HR");
    }

    printValue(display, T1_Y, context.snapshot.find(dryBulbTemperature()));
    printValue(display, T2_Y, context.snapshot.find(rtd2Temperature));
    printValue(display, HR_Y, context.snapshot.find(humidity));
}
