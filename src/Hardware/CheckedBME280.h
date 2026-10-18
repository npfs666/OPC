// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHECKED_BME280_H
#define CHECKED_BME280_H

#include <Adafruit_BME280.h>

/**
 * BME280 dont on peut vérifier qu'il répond.
 *
 * Adafruit_BME280 ignore l'échec d'une lecture I²C (bus bloqué, capteur
 * muet) : readTemperature() et les autres rendent alors une valeur fausse
 * mais plausible. responds() relit l'identifiant de la puce : une lecture
 * ratée laisse l'adresse du registre (0xD0) dans le tampon, jamais
 * l'identifiant lu par begin() (0x60). Les mesures BME l'appellent avant et
 * après chaque lecture.
 */
class CheckedBME280 : public Adafruit_BME280
{
public:
    bool responds()
    {
        return read8(BME280_REGISTER_CHIPID) == sensorID();
    }
};

#endif
