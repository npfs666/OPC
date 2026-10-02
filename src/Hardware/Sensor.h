#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Configurable.h>
#include <Measurements/MeasurementStatus.h>
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

/**
 * Gain de mesure RTD (pleine échelle = Rref / gain) :
 *  - étendue précise, gain 8 : 206 Ω en PT100, soit environ +280 °C ;
 *  - étendue large, gain 4 : 412 Ω, toute la norme (-200 à +850 °C), avec un
 *    pas de quantification deux fois plus grand.
 * Les calibrations (N0, Rref) sont faites en étendue précise.
 */
constexpr uint8_t RTD_PRECISE_GAIN = 8;
constexpr uint8_t RTD_EXTENDED_GAIN = 4;

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

    // Étendue de mesure RTD (voir RTD_PRECISE_GAIN)
    enum class Range : uint8_t
    {
        Precise,
        Extended
    };

    // Settings that can be configured in the menu and needs to be public
    struct Settings
    {
        Type type;
        Wiring wiring;
        double_t offset;
        int32_t samples;
        Physics::Thermocouple::Type thermocoupleType = Physics::Thermocouple::Type::K;
        Range range = Range::Precise;

        // Constante du filtre d'entrée (2e ordre) en secondes, 0 = sans
        // filtre. Appliqué à la température calculée.
        double_t filterTime = 0.0;
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
    void compute();
    bool isAccumulationHalfWay();
    bool isAccumulationDone();
    double_t readValue() const;

    /**
     * État de la dernière acquisition, sans interprétation de la valeur :
     *  - NotReady : aucune acquisition terminée depuis le dernier reset() ;
     *  - Open     : saturation haute de l'ADC. Thermocouple : la polarisation
     *               1 MΩ tire l'entrée ouverte vers la saturation. RTD : la
     *               source de courant sature sur une ligne ouverte ;
     *  - OverRange: RTD en étendue précise, saturée mais lisible au gain de
     *               l'étendue large (voir setRangeDiagnostic()) ;
     *  - Invalid  : saturation basse (câblage inversé) ;
     *  - Ok       : valeur lisible.
     */
    MeasurementStatus acquisitionStatus() const;

    /**
     * RTD : gain de mesure de l'étendue choisie (RTD_PRECISE_GAIN ou
     * RTD_EXTENDED_GAIN), qui relie le code ADC à la résistance.
     */
    static uint8_t measurementGain(const Settings& settings);

    /**
     * RTD : gain programmé dans l'ADC. En 3 fils, Rref voit les deux sources
     * de courant et la sonde une seule : le gain ADC est doublé.
     */
    static uint8_t adcGain(const Settings& settings);

    /**
     * Vrai si l'acquisition en cours a saturé vers le haut en étendue
     * précise : une conversion au gain de l'étendue large dira si l'entrée
     * est ouverte ou seulement au-delà de l'étendue.
     */
    bool needsRangeDiagnostic() const;

    /**
     * Résultat de cette conversion, à donner avant compute() : une entrée
     * lisible au gain large est hors étendue (OverRange), sinon ouverte.
     */
    void setRangeDiagnostic(bool readableAtExtendedGain);

    const SensorBoard* getBoard() const { return board; }

    void registerParameters(ParameterList& list) override;

private:

    friend class SensorBoard;
    SensorBoard* board = nullptr;
    bool saturatedHigh = false;
    bool saturatedLow = false;
    bool readableAtExtendedGain = false;

    int32_t accumulationTarget() const;

    const char* ownerName = "";
    double_t sum;
    //double_t resistance;
    uint16_t sampleCount;
    double_t avgValue = NAN;
    MeasurementStatus lastAcquisition = MeasurementStatus::NotReady;

    void trackSaturation(int32_t value);
};

#endif
