// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DISPLAYABLE_h
#define DISPLAYABLE_h

#include <Arduino.h>

class Displayable
{
public:

    virtual ~Displayable() = default;

    Displayable();

    void begin(const char* name);

    virtual void print(Stream& stream) const;

    bool display = true;

    const char* getName() const;

    virtual double_t printValue() const = 0;

    virtual const char* getUnit() const = 0;

    virtual uint8_t printDecimals() const;

protected:



    const char* name = "";
};

#endif
