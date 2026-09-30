#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>
#include <hmi/Displayable.h>


class Output : public Displayable
{
public:

    Output(const char *name);
    virtual ~Output() = default;

    virtual void begin() = 0;

    virtual void writeCommand(double_t value);

    double_t readCommand() const;

protected:

    // Commande réellement appliquée au matériel
    double_t command;
};

#endif