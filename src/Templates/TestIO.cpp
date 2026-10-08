// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Templates/TestIO.h>

#include <Adafruit_GFX.h>
#include <Hardware/SensorBoard.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <hmi/HomeScreen.h>

#include <cstdio>
#include <cstring>

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

    constexpr const char* COMMAND_KEYS[] = {
        "test_io_command_1", "test_io_command_2"
    };

    const char* operationLabel(LogicCommand::Operation operation)
    {
        switch (operation)
        {
        case LogicCommand::Operation::ForcedOn:
            return "Marche";
        case LogicCommand::Operation::ForcedOff:
            return "Arret ";
        default:
            return "Auto  ";
        }
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

void TestIO::processLogic(uint32_t now)
{
    if (missingWrite && now - missingWriteStart >= MISSING_WRITE_DURATION_MS)
        missingWrite = false;

    for (uint8_t i = 0; i < 2; i++)
    {
        // Oubli simulé : la commande 1 n'est pas écrite.
        if (i == 0 && missingWrite)
            continue;

        // 0,5 donne 50 % au PWM et ON au relais (seuil de ActuatorOnOff).
        // Entrée invalide ou PT100 en défaut : état sûr par dependsOn().
        commands[i].set(inputs[i].isActive() ? 0.5 : 0.0);
    }
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
        return fail("PT100 : entrée analogique indisponible");

    pt100Resistance.begin("Resistance PT100", board, pt100Input);
    pt100Temperature.begin("Temperature PT100", pt100Resistance);

    if (!process.add(pt100Resistance) || !process.add(pt100Temperature))
        return fail("Mesures PT100 non enregistrées");

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
    const char* commandNames[] = {"Commande 1", "Commande 2"};

    PWMOutput::sharedSettings.frequency = 20000;

    for (uint8_t i = 0; i < 2; i++)
    {
        inputs[i].begin(inputNames[i], inputPins[i]);
        commands[i].begin(COMMAND_KEYS[i], commandNames[i]);

        if (!commands[i].dependsOn(inputs[i]))
            return fail("Dépendance commande impossible");

        relayActuators[i].begin(relayNames[i], commands[i]);
        pwmActuators[i].begin(pwmNames[i], commands[i]);
        relays[i].begin(relayNames[i], relayPins[i]);
        pwms[i].begin(pwmNames[i], pwmPins[i]);

        if (!process.add(inputs[i]) || !process.add(commands[i]) ||
            !process.add(relayActuators[i]) || !process.add(pwmActuators[i]) ||
            !process.connect(relayActuators[i], relays[i]) ||
            !process.connect(pwmActuators[i], pwms[i]))
        {
            return fail("Entrées et sorties non reliées");
        }
    }

    // Fonctions fictives de la glue : la commande 2 dépend aussi de la PT100
    // et n'a pas de mode manuel.
    if (!commands[1].dependsOn(pt100Temperature))
        return fail("Dépendance PT100 impossible");

    commands[1].disableManualMode();

    process.registerParameters(parameterList);

    // Essai reproductible : brochage, polarités et fréquence restent fixes.
    // Seul le mode manuel de la commande 1 reste réglable.
    for (size_t i = 0; i < parameterList.count(); i++)
    {
        Parameter* parameter = parameterList.get(i);
        parameter->readOnly =
            std::strcmp(parameter->ownerKey, COMMAND_KEYS[0]) != 0 ||
            std::strcmp(parameter->key, "operation") != 0;
        parameter->persistent = false;
    }

    if (parameterList.hasError())
        return fail("Paramètres TestIO invalides");

    return true;
}

bool TestIO::addMenuActions(MenuBuilder& menu) const
{
    const MenuBuilder::GroupId group =
        menu.findGroupForOwner(COMMAND_KEYS[0]);

    return
        group != MenuBuilder::INVALID_GROUP &&
        menu.addAction(
            group,
            SIMULATE_MISSING_WRITE_ACTION,
            "test_io_missing_write",
            "Simuler oubli glue");
}

bool TestIO::executeMenuAction(MenuBuilder::ActionId actionId)
{
    if (actionId != SIMULATE_MISSING_WRITE_ACTION)
        return false;

    missingWrite = true;
    missingWriteStart = millis();
    return true;
}

void TestIO::onMenuActionSaveFailed(MenuBuilder::ActionId actionId)
{
    if (actionId == SIMULATE_MISSING_WRITE_ACTION)
        missingWrite = false;
}

void TestIO::captureHomeScreenState()
{
    commandOneOperation = commands[0].settings.operation;
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
        display.setCursor(12, 212);
        display.print("Sortie 2 : entree 2 + PT100 Ok");
        display.setCursor(12, 224);
        display.print("Etats commandes, sans retour contact");
        display.setTextSize(2);
    }

    // PT100 et mode de la commande 1, en petit sous l'en-tête.
    {
        const MeasurementSample* pt100 =
            context.snapshot.find(pt100Temperature);

        // Même longueur dans les deux cas : le texte précédent est effacé.
        char text[40];

        if (pt100 != nullptr && pt100->valid)
        {
            std::snprintf(
                text, sizeof(text), "PT100 : %8.1f C  Cde 1 : %s",
                pt100->value, operationLabel(commandOneOperation));
        }
        else
        {
            std::snprintf(
                text, sizeof(text), "PT100 : %-10s  Cde 1 : %s",
                measurementStatusLabel(
                    pt100 != nullptr
                        ? pt100->status
                        : MeasurementStatus::NotReady,
                    10),
                operationLabel(commandOneOperation));
        }

        display.setTextSize(1);
        display.setTextColor(WHITE, BLACK);
        display.setCursor(12, 48);
        display.print(text);
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
