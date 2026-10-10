// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SENSORBOARD_h
#define SENSORBOARD_h

#include<Arduino.h>
#include<Hardware/pinout.h>
#include<Hardware/Sensor.h>
#include<Drivers/ADS1120.h>
#include<Hardware/AcquisitionSequencer.h>
#include<Hardware/AnalogMux.h>
#include <Configurable.h>
#include <hmi/MenuBuilder.h>

/**
 * Carte de mesure : ADS1120, multiplexeurs et entrées.
 *
 * L'ISR DRDY ne fait que lire la conversion. Le séquencement (routage,
 * inversion des courants, diagnostic d'étendue, température de l'ADC) se
 * fait dans loop() par poll(), voir AcquisitionSequencer. Wire1 n'est donc
 * jamais utilisé depuis une interruption.
 */
class SensorBoard: public Configurable, private AcquisitionHardware {

public:

    struct CalibrationProfile
    {
        double_t refResistanceValue;
        double_t systemPPMCoeff;
        double_t calResistanceValue;
        double_t calTemperatureADC;
    };

    struct Settings
    {
        CalibrationProfile pt100 = {
            1650.667,
            7.5,
            100.061,
            26.0
        };

        CalibrationProfile pt1000 = {
            16498.144,
            7.5,
            999.98,
            25.0
        };

        double_t coldJunctionOffset = 0.0;

        double_t zeroOffset[MAX_RTD] = {};
    };

    Settings settings;

    ADS1120   adc;  // ADC
    AnalogMux mux;  // Front end multiplexers
    Sensor* rtd[MAX_RTD]; // Sensor array

    // Un cycle complet est mesuré. Remis à false par l'appelant.
    bool newMeasurement = false;

    SensorBoard();
    bool init();
    bool addSensor(Sensor& sensor);
    uint8_t sensorCount() const { return numSensors; }

    // Acquisition de toutes les entrées, puis de la température de l'ADC.
    void startAcquisition();

    // ADC en single-shot, multiplexeurs et sources de courant coupés.
    void stopAcquisition();

    // loop() : étape suivante de l'acquisition.
    void poll();

    // ISR DRDY : lit la conversion si le séquenceur l'attend.
    void adcInterrupt();

    double_t getAdcTemperature();

    double_t getColdJunctionTemperature() const;
    double_t computeVoltage(const Sensor& sensor) const;

    double_t computeResistance(Sensor& rtdSensor);

    void registerParameters(ParameterList& list) override;
    bool addMenuActions(MenuBuilder& menu) const;
    bool handlesMenuAction(
        MenuBuilder::ActionId actionId) const;
    bool executeMenuAction(
        MenuBuilder::ActionId actionId);
    void onMenuActionSaved(
        MenuBuilder::ActionId actionId);
    void onMenuActionSaveFailed(
        MenuBuilder::ActionId actionId);

private:

    // AcquisitionHardware : appelé par le séquenceur depuis loop().
    void routeInput(
        const Sensor::Settings& settings,
        uint8_t channel) override;
    void disconnectInput(const Sensor::Settings& settings) override;
    void stopConversions() override;
    void invertCurrents() override;
    void selectExtendedGain(const Sensor::Settings& settings) override;
    void beginTemperature() override;
    void endTemperature(int32_t raw) override;
    uint32_t disableInterrupts() override;
    void restoreInterrupts(uint32_t state) override;
    void restartConversions(bool continuous) override;

    void setStandardRTD();
    void setStandardTC();

    struct CalibrationSamples
    {
        double_t average = 0.0;
    };

    static constexpr MenuBuilder::ActionId
        CALIBRATE_ZERO_INPUT_1 = 32;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_ZERO_INPUT_2 = 33;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_ZERO_INPUT_3 = 34;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_PT100_REFERENCE = 35;
    static constexpr MenuBuilder::ActionId
        CALIBRATE_PT1000_REFERENCE = 36;
    static constexpr MenuBuilder::ActionId
        RESET_ZERO_OFFSETS = 37;

    CalibrationProfile& calibrationFor(
        Sensor::Type type);
    const CalibrationProfile& calibrationFor(
        Sensor::Type type) const;

    uint8_t channelFor(
        const Sensor& sensor) const;

    bool calibrateZero(uint8_t channel);
    bool resetZeroOffsets();
    bool calibrateReference(
        Sensor::Type type,
        uint8_t channel);
    bool readCalibrationSamples(
        Sensor::Type type,
        uint8_t channel,
        CalibrationSamples& samples);
    void registerCalibrationParameters(
        ParameterList& list,
        Sensor::Type type);
    void registerZeroCalibrationParameters(
        ParameterList& list);
    void stopCalibrationHardware();

    static uint8_t thermocoupleGain(Physics::Thermocouple::Type type);

    static double_t nominalReferenceResistance(
        Sensor::Type type);

    AcquisitionSequencer sequencer;
    uint8_t numSensors;  // number of RTD sensors in rtd array[]
    double_t adcTemperature;
    Settings calibrationBackup;
    bool calibrationBackupValid = false;
  };

#endif
