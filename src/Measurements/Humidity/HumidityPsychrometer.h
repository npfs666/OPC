// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HUMIDITYPSYCHROMETER_H
#define HUMIDITYPSYCHROMETER_H

#include <Measurements/Humidity/Humidity.h>

class Psychrometer;

/**
 * Humidité relative d'un Psychrometer. Ses réglages (coefficient, bulbe
 * sous 0 °C) sont dans le menu Mesures > <nom>.
 */
class HumidityPsychrometer : public Humidity
{
public:
    HumidityPsychrometer();

    void begin(const char* name, Psychrometer& psychrometer);

    void update() override;

    void registerParameters(ParameterList& list) override;

private:

    Psychrometer* psychrometer = nullptr;
};

#endif
