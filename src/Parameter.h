#ifndef PARAMETER_H
#define PARAMETER_H

#include <Arduino.h>

struct ParameterOption
{
    int32_t value;
    const char* name;
};

struct ParameterDiscreteBinding
{
    void* target;
    int32_t (*read)(const void* target);
    void (*write)(void* target, int32_t value);
};

struct ParameterOwner
{
    /*
     * Identifiant stable de la catégorie.
     *
     * Exemple : "regulators"
     */
    const char* categoryKey = nullptr;

    /*
     * Nom affiché de la catégorie.
     *
     * Exemple : "Regulateur"
     */
    const char* categoryName = nullptr;

    /*
     * Identifiant stable de l'objet propriétaire.
     *
     * Exemple : "thermostat"
     */
    const char* ownerKey = nullptr;

    /*
     * Nom affiché de l'objet propriétaire.
     *
     * Exemple : "Thermostats"
     */
    const char* ownerName = nullptr;
};

struct Parameter
{
    enum class Type : uint8_t
    {
        Bool,
        Integer,
        Double,
        Selection
    };

    const char* categoryKey = nullptr;
    const char* categoryName = nullptr;

    /*
     * Identifiant stable de l'objet propriétaire.
     *
     * Exemple : "thermostat.room"
     */
    const char* ownerKey = nullptr;

    /*
     * Nom affiché de l'objet propriétaire.
     */
    const char* ownerName = nullptr;

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
        double_t* number;

        constexpr Value()
            : boolean(nullptr)
        {
        }
    } value;

    ParameterDiscreteBinding discrete{
        nullptr,
        nullptr,
        nullptr
    };

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

        struct Selection
        {
            const ParameterOption* options;
            uint8_t count;
        } selection;

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
