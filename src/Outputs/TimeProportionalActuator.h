// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIMEPROPORTIONALACTUATOR_H
#define TIMEPROPORTIONALACTUATOR_H

#include <Outputs/Actuator.h>

class TimeProportionalActuator : public Actuator
{
public:

    struct Settings
    {
        uint32_t period;

        /*
         * Durée minimale d'une impulsion et d'une coupure, en ms (0 = sans
         * contrainte) : une impulsion plus courte est supprimée, une coupure
         * plus courte remplacée par une période entière à ON.
         */
        uint32_t minPulse = 0;
    };

    Settings settings;

    TimeProportionalActuator();

    void begin(const char* name,
        Regulator& regulator,
        uint32_t period,
        uint32_t minPulse = 0);

    void begin(
        const char* key,
        const char* name,
        Regulator& regulator,
        uint32_t period,
        uint32_t minPulse = 0);

    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

    /** Impulsion minimale au plus égale à la moitié de la période. */
    bool validateParameters(
        const ParameterEditor& editor) const override;

private:

    uint32_t cycleStart;

    bool relayState;
};

#endif
