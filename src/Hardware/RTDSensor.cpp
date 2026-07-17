#include <Hardware/RTDSensor.h>


RTDSensor::RTDSensor() {

}

RTDSensor::RTDSensor(RTDType type, RTDWiring wiring, uint16_t samples, float_t offset)
 {

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