#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>
#include <hmi/Displayable.h>
#include <Configurable.h>


class Output : public Displayable, Configurable
{
public:

    Output();

    void begin(const char *name);

    virtual ~Output() = default;

    virtual void begin() = 0;

    virtual void writeCommand(double_t value);

    double_t readCommand() const;

    void registerParameters(ParameterList& list) override {};

protected:

    // Commande réellement appliquée au matériel
    double_t command;
};

#endif