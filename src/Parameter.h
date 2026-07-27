#ifndef PARAMETER_H
#define PARAMETER_H

#include <Arduino.h>

struct Parameter
{
    enum class Type : uint8_t
    {
        Bool,
        Integer,
        Double
    };

    /*
     * Identifiant stable de l'objet propriétaire.
     *
     * Exemple : "thermostat.room"
     */
    const char* ownerKey = nullptr;

    /*
     * Identifiant stable du paramètre dans l'objet.
     *
     * Exemple : "setpoint"
     */
    const char* key = nullptr;

    /*
     * Nom affiché à l'utilisateur.
     *
     * Exemple : "Consigne"
     */
    const char* name = nullptr;

    Type type = Type::Bool;

    union Value
    {
        bool* boolean;
        int32_t* integer;
        double_t* number;

        constexpr Value()
            : boolean(nullptr)
        {
        }
    } value;

    union Data
    {
        struct Integer
        {
            int32_t minimum;
            int32_t maximum;
            int32_t step;

            const char* unit;
        } integer;

        struct Number
        {
            double_t minimum;
            double_t maximum;
            double_t step;

            uint8_t decimals;

            const char* unit;
        } number;

        constexpr Data()
            : integer{
                0,
                0,
                1,
                nullptr
            }
        {
        }
    } data;
};

#endif