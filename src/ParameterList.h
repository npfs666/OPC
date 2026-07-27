#ifndef PARAMETER_LIST_H
#define PARAMETER_LIST_H

#include <Arduino.h>

#include <Parameter.h>

class ParameterList
{
public:
    ParameterList() = default;

    /**
     * @brief Initialise la liste avec un tableau statique.
     *
     * Le tableau doit rester valide pendant toute la durée
     * d'utilisation de la liste.
     */
    void begin(
        Parameter* storage,
        size_t capacity);

    /**
     * @brief Vide la liste.
     *
     * Les véritables valeurs référencées ne sont pas modifiées.
     */
    void clear();

    size_t count() const;
    size_t capacity() const;

    bool isEmpty() const;
    bool isFull() const;
    bool isInitialized() const;

    /**
     * @brief Retourne le paramètre à l'index demandé.
     *
     * Retourne nullptr si l'index est invalide.
     */
    const Parameter* get(size_t index) const;

    /**
     * @brief Recherche un paramètre avec son propriétaire et sa clé.
     */
    const Parameter* find(
        const char* ownerKey,
        const char* key) const;

    bool addBool(
        const char* ownerKey,
        const char* key,
        const char* name,
        bool& value);

    bool addInteger(
        const char* ownerKey,
        const char* key,
        const char* name,
        int32_t& value,
        int32_t minimum,
        int32_t maximum,
        int32_t step,
        const char* unit = nullptr);

    bool addDouble(
        const char* ownerKey,
        const char* key,
        const char* name,
        double_t& value,
        double_t minimum,
        double_t maximum,
        double_t step,
        uint8_t decimals,
        const char* unit = nullptr);

private:
    /**
     * @brief Crée la partie commune d'un Parameter.
     *
     * Retourne nullptr si :
     * - la liste n'est pas initialisée ;
     * - la liste est pleine ;
     * - une chaîne est invalide ;
     * - la paire ownerKey/key existe déjà.
     */
    Parameter* create(
        const char* ownerKey,
        const char* key,
        const char* name,
        Parameter::Type type);

    static bool isValidText(const char* text);

    Parameter* parameters = nullptr;

    size_t parameterCapacity = 0;
    size_t parameterCount = 0;
};

#endif