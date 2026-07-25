#include "testInstallation.h"

#include "Hardware/SensorBoard.h"
#include "ProcessControl.h"


TestInstallation::TestInstallation()
{
}

const char* TestInstallation::name() const
{
    return "Test";
}

bool TestInstallation::begin(
    SensorBoard& board,
    Adafruit_BME280& bme,
    ProcessControl& controller)
{
    // ----- Configuration du matériel -----

    board.addRTD(
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0);

    board.addRTD(
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0);

    board.startContinuous();

    // ----- Construction des objets -----
    tempBME.begin("BME", bme);
    humidityBME.begin("BME", bme);
    pressureBME.begin("BME", bme);

    rtd1Resistance.begin(
        "RTD1",
        board,
        board.rtd[0]);

    rtd1Temperature.begin(
        "TempRTD1",
        rtd1Resistance);

    rtd1Temperature.display = true;

    rtd2Resistance.begin(
        "RTD2",
        board,
        board.rtd[1]);

    rtd2Temperature.begin(
        "TempRTD2",
        rtd2Resistance);

    rtd2Temperature.display = true;

    psychrometer.begin(
        rtd1Temperature,
        rtd2Temperature,
        pressureBME);

    psychroHumidity.begin(
        "RH psychrom",
        psychrometer);

    psychroHumidity.display = true;

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
    
   return true;
}

void TestInstallation::buildMenu(MenuBuilder& menu)
{
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