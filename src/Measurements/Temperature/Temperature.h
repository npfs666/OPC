#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Measurements/Measurement.h>
#include <Physics/SecondOrderFilter.h>

class Temperature : public Measurement
{
public:
    Temperature() = default;

    void begin(const char* name) {
        Measurement::begin(name, "°C");
    }

    virtual void update() = 0;

protected:
    /**
     * Filtre d'entrée de constante filterTime (s). Une mesure invalide le
     * remet à zéro : un défaut s'affiche sans retard, et le filtre repart de
     * la première valeur valide.
     */
    void filterValue(uint32_t now, double_t filterTime)
    {
        if (!isValid() || !(filterTime > 0.0))
        {
            filter.reset();
            return;
        }

        const double_t dtSeconds =
            filter.isStarted()
                ? (now - lastFilterTime) / 1000.0
                : 0.0;

        lastFilterTime = now;
        setValue(filter.update(getValue(), dtSeconds, filterTime));
    }

private:
    SecondOrderFilter filter;
    uint32_t lastFilterTime = 0;
};

#endif