#include "testInstallation.h"

#include "Hardware/SensorBoard.h"
#include "MenuBuilder.h"
#include "ProcessControl.h"


TestInstallation::TestInstallation()
{
}

const char* TestInstallation::name() const
{
    return "Installation de test pour le développement";
}

void TestInstallation::printParameters(const ParameterList& list, Stream& stream)
{
    for (size_t i = 0; i < list.count(); i++)
    {
        const Parameter* parameter = list.get(i);

        if (parameter == nullptr)
            continue;

        stream.print(parameter->ownerKey);
        stream.print('.');
        stream.print(parameter->key);
        stream.print(" : ");
        stream.println(parameter->name);
    }
}

bool TestInstallation::begin(SensorBoard &board, Adafruit_BME280 &bme, ProcessControl &controller)
{
    // ----- Configuration du matériel -----
    input1.begin("input1", RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 16, 0);
    board.addRTD(input1);

    input2.begin("input2", RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 16, 0);
    board.addRTD(input2);

    // ----- Construction des objets -----
    tempBME.begin("BME", bme);
    humidityBME.begin("BME", bme);
    pressureBME.begin("BME", bme);

    rtd1Resistance.begin("RTD1", board, input1);
    rtd1Temperature.begin("TempRTD1", rtd1Resistance);

    rtd2Resistance.begin("RTD2", board, input2);
    rtd2Temperature.begin("TempRTD2", rtd2Resistance);

    psychrometer.begin(rtd1Temperature, rtd2Temperature, pressureBME);
    psychroHumidity.begin("RH psychrom",psychrometer);


    // ----- Enregistrement dans le framework -----

    controller.add(tempBME);
    controller.add(humidityBME);
    controller.add(pressureBME);

    controller.add(rtd1Resistance);
    controller.add(rtd1Temperature);

    controller.add(rtd2Resistance);
    controller.add(rtd2Temperature);

    controller.add(psychroHumidity);

    thermostat.begin("Thermostat", rtd2Temperature);
    thermostat.settings.setpoint = 25;

    controller.add(thermostat);

    parameterList.begin(parameterStorage, MAX_PARAMETERS);
    parameterList.clear();
    board.registerParameters(parameterList);
    controller.registerParameters(parameterList);

    printParameters(parameterList, Serial);

    return true;
}

void TestInstallation::buildMenu(MenuBuilder& menu)
{
    const MenuBuilder::GroupId inputMenu =
        menu.addSubmenu(
            menu.root(),
            "Input");

    const MenuBuilder::GroupId input1Menu =
        menu.addSubmenu(
            inputMenu,
            "Input 1");

    const MenuBuilder::GroupId input2Menu =
        menu.addSubmenu(
            inputMenu,
            "Input 2");

    menu.addParameters(
        input1Menu,
        "input1");

    menu.addParameters(
        input2Menu,
        "input2");

    const MenuBuilder::GroupId regulatorMenu =
        menu.addSubmenu(
            menu.root(),
            "Regulateur");

    const MenuBuilder::GroupId thermostatMenu =
        menu.addSubmenu(
            regulatorMenu,
            "Thermostats");

    menu.addParameters(
        thermostatMenu,
        "Thermostat");
}

void TestInstallation::load(Storage& storage)
{
}

void TestInstallation::save(Storage& storage)
{
}

void TestInstallation::factoryReset()
{
}
