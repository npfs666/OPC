#include <Hardware/Sensor.h>


Sensor::Sensor() {

}

Sensor::Sensor(Type type, Wiring wiring, uint16_t samples, float_t offset)
 {

    settings.type = type;
    settings.wiring = wiring;
    settings.samples = samples;
    settings.offset = offset;
    reset();
}
void Sensor::add(int32_t value)
{
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
    if( nMinusOneValue == 0 ) 
        nMinusOneValue = value;
    else
        nMinusOneValue = (double_t) ((ALPHA * value) + (1.0 - ALPHA) * nMinusOneValue);
    //Serial.print(this->val,2); Serial.print(" | ");Serial.println(value);
    //Serial.println(this->val);
    sum += nMinusOneValue;
    sampleCount++;
}
void Sensor::reset()
{
    sum = 0.0;
    sampleCount = 0.0;
    nMinusOneValue = 0.0;
}
void Sensor::compute()
{
    avgValue = (double_t)sum / settings.samples;
    // Serial.print(sum);Serial.print("  |  ");Serial.print(settings.samples);Serial.print("  |  ");Serial.println(sampleCount);
    reset();
}
double_t Sensor::readValue() const
{
    return avgValue;
}

bool Sensor::isAccumulationHalfWay()
{
    if (settings.wiring != Wiring::ThreeWire)
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