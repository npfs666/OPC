#ifndef PARAMETER_H
#define PARAMETER_H

#include <cmath>
#include <cstdint>

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

    /*
     * Les paramètres purement interactifs restent disponibles dans le menu
     * sans être écrits dans la configuration persistante.
     */
    bool persistent = true;

    /*
     * Optionnel : propriétaire dont le sous-menu contiendra celui-ci.
     * Il doit avoir enregistré ses paramètres auparavant.
     *
     * Exemple : "prog_eclairage" pour la plage "prog_eclairage.p1"
     */
    const char* parentOwnerKey = nullptr;

    /*
     * Réglage propre à la carte (calibrations, fuseau horaire) : sauvegardé
     * dans /board.json, conservé quand l'installation ou la configuration
     * change.
     */
    bool board = false;
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

    enum class IntegerFormat : uint8_t
    {
        Plain,
        // Minutes depuis minuit, affichées HH:MM.
        TimeOfDay
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

    /* Voir ParameterOwner::parentOwnerKey. */
    const char* parentOwnerKey = nullptr;

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

    /*
     * Interdit uniquement l'édition par l'utilisateur dans le menu.
     * Le logiciel et la restauration peuvent toujours modifier la valeur.
     */
    bool readOnly = false;

    /* Indique si Storage doit sauvegarder et restaurer ce paramètre. */
    bool persistent = true;

    /* Voir ParameterOwner::board. */
    bool board = false;

    /*
     * Réglage de conduite (consigne, commande manuelle...) : modifié seul,
     * il est appliqué sans pause de l'acquisition ni état sûr des sorties.
     * Voir ParameterList::setLive().
     */
    bool live = false;

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

            IntegerFormat format;
        } integer;

        struct Number
        {
            double_t minimum;
            double_t maximum;
            double_t step;
            // Si positif, step devient le premier pas et tuneStep le pas fin.
            // Zéro conserve le réglage automatique de l'interface.
            double_t tuneStep;

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
                nullptr,
                IntegerFormat::Plain
            }
        {
        }
    } data;
};

#endif
