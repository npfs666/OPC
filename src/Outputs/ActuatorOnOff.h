// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ONOFFACTUATOR_H
#define ONOFFACTUATOR_H

#include <Outputs/Actuator.h>

class ActuatorOnOff : public Actuator
{
public:

    ActuatorOnOff();

    void begin(const char* name, Regulator& regulator);
    void begin(
        const char* key,
        const char* name,
        Regulator& regulator);

    void update(uint32_t now) override;
};

#endif
