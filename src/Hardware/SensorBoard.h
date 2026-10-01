#ifndef SENSORBOARD_h
#define SENSORBOARD_h

#include<Arduino.h>
#include<Hardware/pinout.h>
#include<Hardware/Sensor.h>
#include<Drivers/ADS1120.h>
#include<Hardware/AnalogMux.h>
#include <Configurable.h>
#include <hmi/MenuBuilder.h>
#include <hardware/irq.h>

class SensorBoard: public Configurable {

public:

    /**
     * L'ISR DRDY commande le MCP23017 sur Wire1, bus partagé avec le DS3231
     * et le BMP580. Ce garde masque les IRQ GPIO du cœur courant pendant un
     * accès Wire1 hors ISR. Le front DRDY reste mémorisé et l'ISR s'exécute
     * dès la levée du masque.
     */
    class SharedBusGuard
    {
    public:
        SharedBusGuard()
            : wasEnabled(irq_is_enabled(IO_IRQ_BANK0))
        {
            irq_set_enabled(IO_IRQ_BANK0, false);
        }

        ~SharedBusGuard()
        {
            if (wasEnabled)
                irq_set_enabled(IO_IRQ_BANK0, true);
        }

        SharedBusGuard(const SharedBusGuard&) = delete;
        SharedBusGuard& operator=(const SharedBusGuard&) = delete;

    private:
        const bool wasEnabled;
    };

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
    
    volatile bool newMeasurement=false;    // ADC has finished accumulating values, data is readable
    
    SensorBoard();
    bool init();
    bool addSensor(Sensor& sensor);
    uint8_t sensorCount() const { return numSensors; }
    void startContinuous();
    void pause();
    void restart();
    void invert3WireIDAC();
    void adcInterrupt();
    void setStandardRTD();
    void setStandardTC();
    double_t getAdcTemperature();
    void setWiringRoute(Sensor::Settings settings);

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
    void resetAcquisition();

private:

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
    void stopCalibrationHardware(
        uint8_t channel);

    // Lance la conversion de la température interne de l'ADC sans l'attendre.
    void startAdcTemperature();

    static uint8_t thermocoupleGain(Physics::Thermocouple::Type type);

    static double_t nominalReferenceResistance(
        Sensor::Type type);

    // Termine l'acquisition de la voie courante et passe à la suivante.
    void finishSensor();

    // Conversion au gain de l'étendue large après une saturation en
    // étendue précise (voir Sensor::needsRangeDiagnostic()).
    void startRangeDiagnostic();

    // Nombre de conversions à ignorer après un changement de configuration.
    uint16_t conversionsToDiscard() const;

    uint8_t curSensor;   // cur sensor index
    uint8_t numSensors;  // number of RTD sensors in rtd array[]
    volatile bool pauseInterrupts = true;
    volatile uint16_t discardConversions = 0;
    // Dernière étape du cycle : la prochaine IRQ DRDY lit la température ADC.
    volatile bool measuringAdcTemperature = false;
    // La prochaine conversion utile est la conversion de diagnostic.
    volatile bool diagnosingRange = false;
    double_t adcTemperature;
    Settings calibrationBackup;
    bool calibrationBackupValid = false;
  };

#endif
