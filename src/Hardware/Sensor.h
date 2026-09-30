#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Configurable.h>
#include <Physics/Thermocouple.h>

class SensorBoard;

/**
 * Les RTD sont mesurées à 1000 SPS et non à 20 SPS : à 20 SPS le bruit de
 * l'ADC (~0.12 LSB) est trop faible pour linéariser la quantification
 * (1 LSB ≈ 0.016 °C en PT100 4 fils), la moyenne reste bloquée sur un code et
 * la température évolue en escalier. À 1000 SPS le bruit (~1 LSB) sert de
 * dithering et la moyenne résout bien en dessous du LSB.
 *
 * Le nombre d'échantillons configuré est multiplié par RTD_OVERSAMPLING pour
 * garder la même durée d'intégration (16 × 50 = 800 éch. = 0.8 s = 40 périodes
 * de 50 Hz, ce qui remplace le filtre FIR 50 Hz, inactif à ce débit).
 */
constexpr uint16_t RTD_OVERSAMPLING = 50;

// Établissement après changement de voie : filtre d'entrée à 280 Hz
// (τ ≈ 0.57 ms, ~7.5 ms pour 16 bits), avec marge.
constexpr uint16_t RTD_DISCARDED_CONVERSIONS = 15;

class Sensor : public Configurable
{

public:
    enum class Type : uint8_t
    {
        Pt100,
        Pt1000,
        Tc
    };

    // Measurement method
    enum class Wiring : uint8_t
    {
        TwoWire,
        ThreeWire,
        FourWire
    };

    // Settings that can be configured in the menu and needs to be public
    struct Settings
    {
        Type type;
        Wiring wiring;
        double_t offset;
        int32_t samples;
        Physics::Thermocouple::Type thermocoupleType = Physics::Thermocouple::Type::K;
    };

    Settings settings = {};

    Sensor();

    /**
     * @brief Construct a new Sensor::Sensor object
     *
     * @param type 3 or 4 Wire type
     * @param samples 4 samples -> 1bit improve, 16 -> 2bits, 64 -> 3bits, 256 -> 4bits (oversampling)
     * @param offset Sensor offset in °C
     */
    Sensor(const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset);
    Sensor(const char* key, const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset);

    void begin(const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset);
    void begin(const char* key, const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset);
    void reset();
    void add(int32_t value);
    void addLP(int32_t value);
    void compute();
    bool isAccumulationHalfWay();
    bool isAccumulationDone();
    double_t readValue() const;

    const SensorBoard* getBoard() const { return board; }

    void registerParameters(ParameterList& list) override;

private:

    friend class SensorBoard;
    SensorBoard* board = nullptr;
    bool saturated = false;

    int32_t accumulationTarget() const;

    const char* ownerName = "";
    double_t nMinusOneValue;
    double_t sum;
    //double_t resistance;
    uint16_t sampleCount;
    double_t avgValue = NAN;
};

#endif
