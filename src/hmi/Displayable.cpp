#include <hmi/Displayable.h>


Displayable::Displayable(const char* name) 
    : name(name) 
{
} 

uint8_t Displayable::printDecimals() const {
    return 2;
}

const char* Displayable::getName() const {
    return name;
}

void Displayable::print(Stream& stream) const
{
    PrintSize ps;

    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");

    len = ps.print(printValue(), printDecimals());
    while (len++ < 10)
        stream.print(' ');
    stream.print(printValue(), printDecimals());

    stream.print(' ');

    stream.println(getUnit());
}