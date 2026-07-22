#ifndef DISPLAYABLE_h
#define DISPLAYABLE_h

#include <Arduino.h>
#include <PrintSize.h>

class Displayable
{
public:

    virtual ~Displayable() = default;

    Displayable(const char* name);

    virtual void print(Stream& stream) const;

protected:

    const char* getName() const;

    virtual double_t printValue() const = 0;

    virtual const char* getUnit() const = 0;

    virtual uint8_t printDecimals() const;

    const char* name;
};

#endif