#include "TestHarness.h"

#include <Hardware/SensorBoard.h>
#include <Measurements/MeasurementStatus.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Temperature/TemperatureTC.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <hmi/MeasurementDisplay.h>

#include <cstring>
#include <limits>

namespace
{
    using Status = MeasurementStatus;

    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

    constexpr Status ALL_STATUSES[] = {
        Status::NotReady, Status::Ok, Status::Open, Status::Short,
        Status::UnderRange, Status::OverRange, Status::Invalid
    };

    // Termine une acquisition du capteur avec une seule valeur ADC.
    void acquire(Sensor& sensor, int32_t adcValue)
    {
        sensor.add(adcValue);
        sensor.compute();
    }

    void testLabels()
    {
        CHECK_TRUE(std::strcmp(measurementStatusLabel(Status::Open), "RUPTURE") == 0);
        CHECK_TRUE(std::strcmp(measurementStatusLabel(Status::Open, 4), "RUPT") == 0);
        CHECK_TRUE(std::strcmp(measurementStatusLabel(Status::Short, 9), "C-CIRCUIT") == 0);
        CHECK_TRUE(std::strcmp(measurementStatusLabel(Status::Short, 8), "C-C") == 0);
        CHECK_TRUE(std::strcmp(measurementStatusLabel(Status::OverRange, 7), "HAUT") == 0);

        for (const Status status : ALL_STATUSES)
        {
            CHECK_TRUE(std::strlen(measurementStatusLabel(status)) <= 10);
            CHECK_TRUE(std::strlen(measurementStatusLabel(status, 0)) <= 4);
        }
    }

    void testSensorAcquisitionStatus()
    {
        Sensor sensor("RTD", Sensor::Type::Pt100,
                      Sensor::Wiring::FourWire, 16, 0.0f);
        CHECK_TRUE(sensor.acquisitionStatus() == Status::NotReady);

        acquire(sensor, 1000);
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Ok);
        CHECK_NEAR(sensor.readValue(), 1000, 0);

        // Une seule saturation haute suffit : ligne ouverte.
        sensor.add(0);
        acquire(sensor, 32767);
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Open);
        CHECK_TRUE(std::isnan(sensor.readValue()));

        acquire(sensor, -32768);
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Invalid);
        CHECK_TRUE(std::isnan(sensor.readValue()));

        // Les deux saturations : la rupture prime.
        sensor.add(-32768);
        acquire(sensor, 32767);
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Open);

        sensor.compute();
        CHECK_TRUE(sensor.acquisitionStatus() == Status::NotReady);

        acquire(sensor, 1000);
        sensor.reset();
        CHECK_TRUE(sensor.acquisitionStatus() == Status::NotReady);
    }

    void testRangeGains()
    {
        Sensor::Settings settings{
            Sensor::Type::Pt100, Sensor::Wiring::FourWire, 0.0, 16};
        CHECK_TRUE(settings.range == Sensor::Range::Precise);
        CHECK_TRUE(Sensor::measurementGain(settings) == 8);
        CHECK_TRUE(Sensor::adcGain(settings) == 8);

        // 3 fils : Rref voit les deux sources de courant, gain ADC doublé.
        settings.wiring = Sensor::Wiring::ThreeWire;
        CHECK_TRUE(Sensor::measurementGain(settings) == 8);
        CHECK_TRUE(Sensor::adcGain(settings) == 16);

        settings.range = Sensor::Range::Extended;
        CHECK_TRUE(Sensor::measurementGain(settings) == 4);
        CHECK_TRUE(Sensor::adcGain(settings) == 8);

        settings.wiring = Sensor::Wiring::TwoWire;
        CHECK_TRUE(Sensor::measurementGain(settings) == 4);
        CHECK_TRUE(Sensor::adcGain(settings) == 4);
    }

    void testRangeDiagnostic()
    {
        Sensor sensor("RTD", Sensor::Type::Pt100,
                      Sensor::Wiring::FourWire, 16, 0.0f);

        sensor.add(1000);
        CHECK_FALSE(sensor.needsRangeDiagnostic());

        // Étendue précise saturée, lisible au gain large : hors étendue.
        sensor.add(32767);
        CHECK_TRUE(sensor.needsRangeDiagnostic());
        sensor.setRangeDiagnostic(true);
        sensor.compute();
        CHECK_TRUE(sensor.acquisitionStatus() == Status::OverRange);
        CHECK_TRUE(std::isnan(sensor.readValue()));

        // Le résultat ne vaut que pour l'acquisition en cours.
        acquire(sensor, 32767);
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Open);

        sensor.add(32767);
        sensor.setRangeDiagnostic(false);
        sensor.compute();
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Open);

        // Étendue large : une saturation est forcément une rupture.
        sensor.settings.range = Sensor::Range::Extended;
        sensor.add(32767);
        CHECK_FALSE(sensor.needsRangeDiagnostic());
        sensor.compute();
        CHECK_TRUE(sensor.acquisitionStatus() == Status::Open);

        Sensor thermocouple;
        thermocouple.begin("TC", Sensor::Type::Tc,
                           Sensor::Wiring::TwoWire, 16, 0);
        thermocouple.add(32767);
        CHECK_FALSE(thermocouple.needsRangeDiagnostic());

        // La température RTD affiche le dépassement.
        SensorBoard board;
        board.resistanceOhms = NaN;
        sensor.settings.range = Sensor::Range::Precise;
        Resistance resistance;
        resistance.begin("Resistance", board, sensor);
        TemperatureRTD temperature;
        temperature.begin("Temperature", resistance);

        sensor.add(32767);
        sensor.setRangeDiagnostic(true);
        sensor.compute();
        resistance.update();
        temperature.update();
        CHECK_TRUE(temperature.getStatus() == Status::OverRange);
    }

    void testResistanceClassification()
    {
        SensorBoard board;
        Sensor sensor("RTD", Sensor::Type::Pt100,
                      Sensor::Wiring::FourWire, 16, 0.0f);
        Resistance resistance;
        resistance.begin("Resistance", board, sensor);
        CHECK_TRUE(resistance.getStatus() == Status::NotReady);

        const auto statusFor = [&](double ohms)
        {
            board.resistanceOhms = ohms;
            resistance.update();
            return resistance.getStatus();
        };

        // PT100 : court-circuit < 10 Ω, étendue 18,52 à 390,48 Ω.
        CHECK_TRUE(statusFor(-3.0) == Status::Short);
        CHECK_TRUE(statusFor(0.5) == Status::Short);
        CHECK_TRUE(statusFor(9.99) == Status::Short);
        CHECK_TRUE(statusFor(15.0) == Status::UnderRange);
        CHECK_TRUE(statusFor(18.53) == Status::Ok);
        CHECK_TRUE(statusFor(100.0) == Status::Ok);
        CHECK_TRUE(statusFor(390.4) == Status::Ok);
        CHECK_TRUE(statusFor(390.5) == Status::OverRange);
        CHECK_NEAR(resistance.getValue(), 390.5, 0);

        sensor.settings.type = Sensor::Type::Pt1000;
        CHECK_TRUE(statusFor(95.0) == Status::Short);
        CHECK_TRUE(statusFor(150.0) == Status::UnderRange);
        CHECK_TRUE(statusFor(1000.0) == Status::Ok);
        CHECK_TRUE(statusFor(3905.0) == Status::OverRange);

        sensor.settings.type = Sensor::Type::Tc;
        CHECK_TRUE(statusFor(100.0) == Status::Invalid);
        sensor.settings.type = Sensor::Type::Pt100;

        // Pas de résistance : la cause vient de l'acquisition.
        sensor.reset();
        CHECK_TRUE(statusFor(NaN) == Status::NotReady);

        acquire(sensor, 32767);
        CHECK_TRUE(statusFor(NaN) == Status::Open);

        acquire(sensor, -32768);
        CHECK_TRUE(statusFor(NaN) == Status::Invalid);

        acquire(sensor, 1000);
        CHECK_TRUE(statusFor(NaN) == Status::Invalid);
        CHECK_TRUE(statusFor(std::numeric_limits<double>::infinity()) ==
                   Status::Invalid);

        Resistance unbound;
        unbound.update();
        CHECK_TRUE(unbound.getStatus() == Status::Invalid);
    }

    void testRtdTemperaturePropagation()
    {
        SensorBoard board;
        Sensor sensor("RTD", Sensor::Type::Pt100,
                      Sensor::Wiring::ThreeWire, 16, 0.0f);
        Resistance resistance;
        resistance.begin("Resistance", board, sensor);
        TemperatureRTD temperature;
        temperature.begin("Temperature", resistance);
        CHECK_TRUE(temperature.getStatus() == Status::NotReady);

        const auto statusFor = [&](double ohms)
        {
            board.resistanceOhms = ohms;
            resistance.update();
            temperature.update();
            return temperature.getStatus();
        };

        CHECK_TRUE(statusFor(100.0) == Status::Ok);
        CHECK_TRUE(statusFor(1.0) == Status::Short);
        CHECK_TRUE(statusFor(17.0) == Status::UnderRange);
        CHECK_TRUE(statusFor(400.0) == Status::OverRange);

        acquire(sensor, 32767);
        CHECK_TRUE(statusFor(NaN) == Status::Open);
        CHECK_FALSE(temperature.isValid());

        CHECK_TRUE(statusFor(138.506) == Status::Ok);
        CHECK_NEAR(temperature.getValue(), 100.0, 0.005);
    }

    void testThermocoupleStatus()
    {
        SensorBoard board;
        Sensor sensor;
        sensor.begin("input", Sensor::Type::Tc, Sensor::Wiring::TwoWire, 16, 0);
        CHECK_TRUE(board.addSensor(sensor));

        TemperatureTC temperature;
        temperature.begin("TC", sensor);
        CHECK_TRUE(temperature.getStatus() == Status::NotReady);

        board.adcTemperature = 20.0;

        const auto statusFor = [&](double millivolts)
        {
            board.voltageMv = millivolts;
            temperature.update();
            return temperature.getStatus();
        };

        CHECK_TRUE(statusFor(NaN) == Status::NotReady);

        // Entrée ouverte : la polarisation 1 MΩ sature l'ADC.
        acquire(sensor, 32767);
        CHECK_TRUE(statusFor(NaN) == Status::Open);
        CHECK_TRUE(std::isnan(temperature.getValue()));

        acquire(sensor, -32768);
        CHECK_TRUE(statusFor(NaN) == Status::Invalid);

        // Type K : domaine -270 à 1372 °C (-6,458 à 54,886 mV).
        CHECK_TRUE(statusFor(3.298) == Status::Ok);
        CHECK_TRUE(statusFor(60.0) == Status::OverRange);
        CHECK_TRUE(statusFor(-10.0) == Status::UnderRange);

        // Jonction froide inconnue : cause non attribuable à la pointe.
        board.adcTemperature = NaN;
        CHECK_TRUE(statusFor(3.298) == Status::Invalid);

        board.adcTemperature = 20.0;
        CHECK_TRUE(statusFor(3.298) == Status::Ok);

        temperature.begin("TC", sensor);
        CHECK_TRUE(temperature.getStatus() == Status::NotReady);
    }

    void testDisplayFormat()
    {
        char text[32];

        MeasurementSample sample;
        sample.status = Status::Ok;
        sample.valid = true;
        sample.value = 23.456;
        sample.decimals = 1;
        sample.unit = "°C";

        // "°" est converti en CP437 (0xF8).
        CHECK_TRUE(MeasurementDisplay::format(&sample, text, sizeof(text)) == 7);
        CHECK_TRUE(std::strcmp(text, "23.5 \xF8" "C") == 0);
        CHECK_TRUE(MeasurementDisplay::color(&sample, 0x1234) == 0x1234);

        sample.unit = "";
        sample.decimals = 0;
        MeasurementDisplay::format(&sample, text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "23") == 0);

        sample.value = NaN;
        MeasurementDisplay::format(&sample, text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "ERREUR") == 0);
        CHECK_TRUE(MeasurementDisplay::color(&sample, 0x1234) ==
                   MeasurementDisplay::COLOR_FAULT);

        sample.status = Status::Open;
        sample.valid = false;
        MeasurementDisplay::format(&sample, text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "RUPTURE") == 0);
        MeasurementDisplay::format(&sample, text, sizeof(text), 4);
        CHECK_TRUE(std::strcmp(text, "RUPT") == 0);
        CHECK_TRUE(MeasurementDisplay::color(&sample, 0x1234) ==
                   MeasurementDisplay::COLOR_FAULT);

        sample.status = Status::OverRange;
        CHECK_TRUE(MeasurementDisplay::color(&sample, 0x1234) ==
                   MeasurementDisplay::COLOR_OUT_OF_RANGE);

        sample.status = Status::NotReady;
        MeasurementDisplay::format(&sample, text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "...") == 0);
        CHECK_TRUE(MeasurementDisplay::color(&sample, 0x1234) ==
                   MeasurementDisplay::COLOR_NOT_READY);

        // Snapshot vide au démarrage ou après le menu : en attente.
        MeasurementDisplay::format(nullptr, text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "...") == 0);
        CHECK_TRUE(MeasurementDisplay::color(nullptr, 0x1234) ==
                   MeasurementDisplay::COLOR_NOT_READY);

        // Tampon trop court : texte tronqué, toujours terminé.
        sample.status = Status::Ok;
        sample.value = 123.456;
        sample.decimals = 2;
        sample.unit = "°C";
        char small[5];
        MeasurementDisplay::format(&sample, small, sizeof(small));
        CHECK_TRUE(std::strlen(small) == 4);
    }

    void testStatusEventsAndSnapshot()
    {
        SensorBoard board;
        Sensor sensor;
        sensor.begin("input", Sensor::Type::Tc, Sensor::Wiring::TwoWire, 16, 0);
        CHECK_TRUE(board.addSensor(sensor));
        board.adcTemperature = 20.0;
        board.voltageMv = 3.298;

        TemperatureTC temperature;
        temperature.begin("TC", sensor);
        ProcessControl process;
        CHECK_TRUE(process.add(temperature));

        Stream log;
        ProcessSnapshot snapshot;

        // Démarrage : NotReady -> Ok n'est pas journalisé.
        process.updateMeasurementsAndRegulators(1000);
        process.printStatusEvents(log);
        CHECK_TRUE(log.printedLineCount == 0);

        acquire(sensor, 32767);
        board.voltageMv = NaN;
        process.updateMeasurementsAndRegulators(2000);
        process.captureSnapshot(snapshot, 2000);
        const MeasurementSample* sample = snapshot.find(temperature);
        CHECK_TRUE(sample != nullptr);
        if (sample != nullptr)
        {
            CHECK_TRUE(sample->status == Status::Open);
            CHECK_FALSE(sample->valid);
        }

        // Un état stable n'est journalisé qu'une fois.
        process.updateMeasurementsAndRegulators(3000);
        process.printStatusEvents(log);
        CHECK_TRUE(log.printedLineCount == 1);

        process.printStatusEvents(log);
        CHECK_TRUE(log.printedLineCount == 1);

        // Au-delà de 8 changements non lus : une ligne de synthèse.
        log.printedLineCount = 0;
        for (uint32_t i = 0; i < 10; i++)
        {
            board.voltageMv = i % 2 == 0 ? 3.298 : NaN;
            process.updateMeasurementsAndRegulators(4000 + i);
        }
        process.printStatusEvents(log);
        CHECK_TRUE(log.printedLineCount == 9);
    }
}

void runMeasurementStatusTests()
{
    TestHarness::run("etat de mesure : libelles", testLabels);
    TestHarness::run("etat de mesure : acquisition", testSensorAcquisitionStatus);
    TestHarness::run("etendue RTD : gains", testRangeGains);
    TestHarness::run("etendue RTD : diagnostic de saturation", testRangeDiagnostic);
    TestHarness::run("etat de mesure : resistance RTD", testResistanceClassification);
    TestHarness::run("etat de mesure : propagation RTD", testRtdTemperaturePropagation);
    TestHarness::run("etat de mesure : thermocouple", testThermocoupleStatus);
    TestHarness::run("etat de mesure : affichage", testDisplayFormat);
    TestHarness::run("etat de mesure : journal et snapshot", testStatusEventsAndSnapshot);
}
