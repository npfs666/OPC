#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>


class Output
{
public:

    Output(const char *name);
    virtual ~Output() = default;

    virtual void begin() = 0;

    virtual void writeCommand(double_t value);

    double_t readCommand() const;

    const char *name() const;

protected:

    const char *outputName;

    // Commande réellement appliquée au matériel
    double_t command;
};

#endif