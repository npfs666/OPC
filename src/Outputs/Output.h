#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>
#include <hmi/Displayable.h>
#include <Configurable.h>


class Output : public Displayable, public Configurable
{
public:

    Output();

    void begin(const char* name);
    void begin(
        const char* key,
        const char* name);

    virtual ~Output() = default;

    virtual void begin() = 0;

    virtual void writeCommand(double_t value);

    double_t readCommand() const;

    void registerParameters(ParameterList& list) override {};

protected:

    const char* getKey() const;

    double_t printValue() const override;
    const char* getUnit() const override;

    const char* key = "";

    // Commande réellement appliquée au matériel
    double_t command = 0.0;
};

#endif
