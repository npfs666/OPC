/**
 * Programme minimal de mesure psychrometrique pour les essais CSV.
 *
 * RTD1 : temperature seche
 * RTD2 : temperature humide
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include <Adafruit_BME280.h>

#include <Hardware/Sensor.h>
#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <Measurements/Humidity/HumidityPsychrometer.h>
#include <Measurements/Pressure/PressureBME.h>
#include <Measurements/Psychrometer.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

namespace
{
    SensorBoard sensorBoard;
    Adafruit_BME280 bme;

    Sensor drySensor;
    Sensor wetSensor;

    Resistance dryResistance;
    Resistance wetResistance;

    TemperatureRTD dryTemperature;
    TemperatureRTD wetTemperature;

    PressureBME pressure;
    Psychrometer psychrometer;
    HumidityPsychrometer psychrometricHumidity;

    void adcInterrupt()
    {
        sensorBoard.adcInterrupt();

        // Conserve un jeu de mesures coherent jusqu'a l'impression suivante.
        if (sensorBoard.newMeasurement)
            sensorBoard.pause();
    }

    void stopWithError(const char* message)
    {
        Serial.println(message);

        while (true)
            delay(1000);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Ameliore la stabilite des mesures de la carte OPC.
    pinMode(23, OUTPUT);
    digitalWrite(23, HIGH);

    Wire.setSDA(BME_SDA);
    Wire.setSCL(BME_SCL);

    if (!bme.begin(0x76, &Wire))
        stopWithError("Erreur initialisation BME280");

    bme.setSampling(
        Adafruit_BME280::MODE_NORMAL,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::FILTER_OFF,
        Adafruit_BME280::STANDBY_MS_0_5);

    sensorBoard.init();

    drySensor.begin(
        "input1",
        "Input 1",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0);

    wetSensor.begin(
        "input2",
        "Input 2",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0);

    if (!sensorBoard.addSensor(drySensor) ||
        !sensorBoard.addSensor(wetSensor))
    {
        stopWithError("Erreur initialisation PT100");
    }

    dryResistance.begin("RTD1", sensorBoard, drySensor);
    dryTemperature.begin("Temperature seche", dryResistance);

    wetResistance.begin("RTD2", sensorBoard, wetSensor);
    wetTemperature.begin("Temperature humide", wetResistance);

    pressure.begin("Pression", bme);
    psychrometer.begin(dryTemperature, wetTemperature, pressure);
    psychrometricHumidity.begin(
        "Humidite psychrometrique",
        psychrometer);

    attachInterrupt(
        digitalPinToInterrupt(ADC_DRDY),
        adcInterrupt,
        FALLING);

    sensorBoard.startContinuous();

    Serial.println(
        "temps_ms;temperature_humide_C;humidite_psychrometrique_pct");
}

void loop()
{
    delay(5000);

    if (!sensorBoard.newMeasurement)
        return;

    sensorBoard.newMeasurement = false;

    dryResistance.update();
    dryTemperature.update();
    wetResistance.update();
    wetTemperature.update();
    pressure.update();
    psychrometricHumidity.update();

    Serial.print(millis());
    Serial.print(';');

    if (wetTemperature.isValid())
        Serial.print(wetTemperature.getValue(), 2);
    else
        Serial.print("nan");

    Serial.print(';');

    if (psychrometricHumidity.isValid())
        Serial.println(psychrometricHumidity.getValue(), 2);
    else
        Serial.println("nan");

    sensorBoard.restart();
}
