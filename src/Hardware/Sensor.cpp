#include <Hardware/Sensor.h>

#include <Hardware/pinout.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr ParameterOption TYPE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Sensor::Type::Pt100),
            "PT100"
        }
    };

    constexpr ParameterOption WIRING_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Sensor::Wiring::ThreeWire),
            "3 fils"
        },
        {
            static_cast<int32_t>(
                Sensor::Wiring::FourWire),
            "4 fils"
        }
    };

    constexpr ParameterOption SAMPLE_OPTIONS[] = {
        {2, "2"},
        {4, "4"},
        {8, "8"},
        {16, "16"},
        {32, "32"},
        {64, "64"},
        {128, "128"}
    };
}


Sensor::Sensor() {

}

Sensor::Sensor(const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset)
{
    begin(
        name,
        name,
        type,
        wiring,
        samples,
        offset);
}

Sensor::Sensor(
    const char* key,
    const char* name,
    Type type,
    Wiring wiring,
    uint16_t samples,
    float_t offset)
{
    begin(
        key,
        name,
        type,
        wiring,
        samples,
        offset);
}

/**
 * @brief Adds a sensor to the list
 * 
 * @param number id
 * @param type TYPE_2WIRE TYPE_3WIRE TYPE_4WIRE
 * @param switchPin mux pin
 * @param samples 4 samples -> 1bit improve, 16 -> 2bits, 64 -> 3bits, 256 -> 4bits (oversampling)
 * @param offset Sensor offset
 */
void Sensor::begin(const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset)
{
    begin(
        name,
        name,
        type,
        wiring,
        samples,
        offset);
}

void Sensor::begin(
    const char* key,
    const char* name,
    Type type,
    Wiring wiring,
    uint16_t samples,
    float_t offset)
{
    beginConfiguration(key);
    ownerName = name;
    settings.type = type;
    settings.wiring = wiring;
    settings.samples = samples;
    settings.offset = offset;
    reset();
}

void Sensor::add(int32_t value)
{
    saturated |= value <= -32768 || value >= 32767;
    sum += value;
    sampleCount++;
}
/**
 * Low pass EMA (exponential moving average) Filter
 * 
 * @param value Last adc read value
 */
void Sensor::addLP(int32_t value)
{
    saturated |= value <= -32768 || value >= 32767;
    if( nMinusOneValue == 0 ) 
        nMinusOneValue = value;
    else
        nMinusOneValue = (double_t) ((ALPHA * value) + (1.0 - ALPHA) * nMinusOneValue);

    sum += nMinusOneValue;
    sampleCount++;
}
void Sensor::reset()
{
    avgValue = NAN;
    saturated = false;
    sum = 0.0;
    sampleCount = 0.0;
    nMinusOneValue = 0.0;
}
void Sensor::compute()
{
    const double_t result = sampleCount > 0 && !saturated
        ? sum / sampleCount : NAN;
    reset();
    avgValue = result;
}
double_t Sensor::readValue() const
{
    return avgValue;
}

bool Sensor::isAccumulationHalfWay()
{
    if (settings.type == Type::Tc || settings.wiring != Wiring::ThreeWire)
        return false;

    if (sampleCount == (settings.samples / 2))
        return true;
    else
        return false;
}

bool Sensor::isAccumulationDone()
{
    if (sampleCount == settings.samples)
        return true;
    else
        return false;
}

void Sensor::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "inputs",
        "Input",
        getConfigurationKey(),
        ownerName
    });

    if (settings.type == Type::Tc)
    {
        using Type = Physics::Thermocouple::Type;
        static constexpr ParameterOption types[] = {
            {int32_t(Type::B), "B"}, {int32_t(Type::E), "E"},
            {int32_t(Type::J), "J"}, {int32_t(Type::K), "K"},
            {int32_t(Type::N), "N"}, {int32_t(Type::R), "R"},
            {int32_t(Type::S), "S"}, {int32_t(Type::T), "T"}
        };
        parameters.addSelection("tc.type", "Thermocouple", settings.thermocoupleType, types);
    }
    else
    {
        parameters.addSelection(
            "sensor.type",
            "Type",
            settings.type,
            TYPE_OPTIONS);

        parameters.addSelection(
            "sensor.wiring",
            "Câblage",
            settings.wiring,
            WIRING_OPTIONS);

    }

    parameters.addDouble(
        "sensor.offset",
        "Offset",
        settings.offset,
        -5,
        5,
        0.01,
        3,
        "°C");

    parameters.addSelection(
        "sensor.samples",
        "Samples",
        settings.samples,
        SAMPLE_OPTIONS);
}
