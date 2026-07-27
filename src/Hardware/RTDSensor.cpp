#include <Hardware/RTDSensor.h>

namespace
{
    constexpr ParameterOption RTD_TYPE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                RTDSensor::RTDType::Pt100),
            "PT100"
        }
    };

    constexpr ParameterOption RTD_WIRING_OPTIONS[] = {
        {
            static_cast<int32_t>(
                RTDSensor::RTDWiring::ThreeWire),
            "3 fils"
        },
        {
            static_cast<int32_t>(
                RTDSensor::RTDWiring::FourWire),
            "4 fils"
        }
    };

    constexpr ParameterOption RTD_SAMPLE_OPTIONS[] = {
        {2, "2"},
        {4, "4"},
        {8, "8"},
        {16, "16"},
        {32, "32"},
        {64, "64"},
        {128, "128"}
    };
}


RTDSensor::RTDSensor() {

}

RTDSensor::RTDSensor(const char* name, RTDType type, RTDWiring wiring, uint16_t samples, float_t offset)
{
    begin(
        name,
        name,
        type,
        wiring,
        samples,
        offset);
}

RTDSensor::RTDSensor(
    const char* key,
    const char* name,
    RTDType type,
    RTDWiring wiring,
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

void RTDSensor::begin(const char* name, RTDType type, RTDWiring wiring, uint16_t samples, float_t offset)
{
    begin(
        name,
        name,
        type,
        wiring,
        samples,
        offset);
}

void RTDSensor::begin(
    const char* key,
    const char* name,
    RTDType type,
    RTDWiring wiring,
    uint16_t samples,
    float_t offset)
{
    ownerKey = key;
    ownerName = name;
    settings.type = type;
    settings.wiring = wiring;
    settings.samples = samples;
    settings.offset = offset;
    reset();
}

void RTDSensor::add(int32_t value)
{
    sum += value;
    sampleCount++;
}
/**
 * Low pass EMA (exponential moving average) Filter
 * 
 * @param value Last adc read value
 */
void RTDSensor::addLP(int32_t value)
{
    if( nMinusOneValue == 0 ) 
        nMinusOneValue = value;
    else
        nMinusOneValue = (double_t) ((ALPHA * value) + (1.0 - ALPHA) * nMinusOneValue);
    //Serial.print(this->val,2); Serial.print(" | ");Serial.println(value);
    //Serial.println(this->val);
    sum += nMinusOneValue;
    sampleCount++;
}
void RTDSensor::reset()
{
    sum = 0.0;
    sampleCount = 0.0;
    nMinusOneValue = 0.0;
}
void RTDSensor::compute()
{
    avgValue = (double_t)sum / settings.samples;
    // Serial.print(sum);Serial.print("  |  ");Serial.print(settings.samples);Serial.print("  |  ");Serial.println(sampleCount);
    reset();
}
double_t RTDSensor::readValue() const
{
    return avgValue;
}

bool RTDSensor::isAccumulationHalfWay()
{
    if (settings.wiring != RTDWiring::ThreeWire)
        return false;

    if (sampleCount == (settings.samples / 2))
        return true;
    else
        return false;
}

bool RTDSensor::isAccumulationDone()
{
    if (sampleCount == settings.samples)
        return true;
    else
        return false;
}

void RTDSensor::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "inputs",
        "Input",
        ownerKey,
        ownerName
    });

    parameters.addSelection(
        "rtd.type",
        "Type",
        settings.type,
        RTD_TYPE_OPTIONS);

    parameters.addSelection(
        "rtd.wiring",
        "Câblage",
        settings.wiring,
        RTD_WIRING_OPTIONS);

    parameters.addDouble(
        "rtd.offset",
        "Offset",
        settings.offset,
        -5,
        5,
        0.01,
        3,
        "°C");

    parameters.addSelection(
        "rtd.samples",
        "Samples",
        settings.samples,
        RTD_SAMPLE_OPTIONS);
}
