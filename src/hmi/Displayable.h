#ifndef DISPLAYABLE_h
#define DISPLAYABLE_h

#include <Arduino.h>
#include <PrintSize.h>

class Displayable
{
public:

    virtual ~Displayable() = default;

    Displayable();

    void begin(const char* name);

    virtual void print(Stream& stream) const;

    bool display = true;

protected:

    const char* getName() const;

    virtual double_t printValue() const = 0;

    virtual const char* getUnit() const = 0;

    virtual uint8_t printDecimals() const;

    const char* name = "";
};

#endif