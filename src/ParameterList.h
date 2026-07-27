#ifndef PARAMETER_LIST_H
#define PARAMETER_LIST_H

#include <Arduino.h>

#include <Hardware/pinout.h>
#include <Parameter.h>

#include <limits>
#include <type_traits>

template<typename Discrete, bool IsEnum>
struct ParameterDiscreteStorage
{
    using Type = Discrete;
};

template<typename Discrete>
struct ParameterDiscreteStorage<Discrete, true>
{
    using Type =
        typename std::underlying_type<
            Discrete>::type;
};

class ParameterList
{
public:
    static constexpr size_t MAX_SELECTION_OPTIONS =
        2 * MAX_PARAMETERS;

    class Writer
    {
    public:
        bool addBool(
            const char* key,
            const char* name,
            bool& value);

        template<typename Integer>
        bool addInteger(
            const char* key,
            const char* name,
            Integer& value,
            int32_t minimum,
            int32_t maximum,
            int32_t step,
            const char* unit = nullptr)
        {
            static_assert(
                std::is_integral<Integer>::value &&
                !std::is_same<Integer, bool>::value,
                "addInteger requires an integer type");

            static_assert(
                sizeof(Integer) <= sizeof(uint32_t),
                "addInteger only supports integers up to 32 bits");

            static_assert(
                !std::is_const<Integer>::value &&
                !std::is_volatile<Integer>::value,
                "addInteger requires a mutable target");

            if (list == nullptr)
                return false;

            if (!fitsIntegerRange(
                    value,
                    minimum,
                    maximum))
            {
                return list->remember(false);
            }

            const ParameterDiscreteBinding binding{
                &value,
                &readDiscrete<Integer>,
                &writeDiscrete<Integer>
            };

            return list->remember(
                list->addInteger(
                    owner,
                    key,
                    name,
                    binding,
                    minimum,
                    maximum,
                    step,
                    unit));
        }

        bool addDouble(
            const char* key,
            const char* name,
            double_t& value,
            double_t minimum,
            double_t maximum,
            double_t step,
            uint8_t decimals,
            const char* unit = nullptr);

        template<typename Discrete, size_t OptionCount>
        bool addSelection(
            const char* key,
            const char* name,
            Discrete& value,
            const ParameterOption (&options)[OptionCount])
        {
            static_assert(
                (std::is_enum<Discrete>::value ||
                 std::is_integral<Discrete>::value) &&
                !std::is_same<Discrete, bool>::value,
                "addSelection requires an enum or integer type");

            static_assert(
                sizeof(Discrete) <= sizeof(uint32_t),
                "addSelection only supports values up to 32 bits");

            static_assert(
                !std::is_const<Discrete>::value &&
                !std::is_volatile<Discrete>::value,
                "addSelection requires a mutable target");

            static_assert(
                OptionCount > 0 &&
                OptionCount <= UINT8_MAX,
                "invalid selection option count");

            if (list == nullptr)
                return false;

            if (!isDiscreteValueSupported(value) ||
                !doSelectionOptionsFit<Discrete>(
                    options))
            {
                return list->remember(false);
            }

            const ParameterDiscreteBinding binding{
                &value,
                &readDiscrete<Discrete>,
                &writeDiscrete<Discrete>
            };

            return list->remember(
                list->addSelection(
                    owner,
                    key,
                    name,
                    binding,
                    options,
                    static_cast<uint8_t>(
                        OptionCount)));
        }

    private:
        friend class ParameterList;

        Writer(
            ParameterList& list,
            const ParameterOwner& owner);

        ParameterList* list = nullptr;
        ParameterOwner owner;

        template<typename Discrete>
        static int32_t readDiscrete(
            const void* target)
        {
            return static_cast<int32_t>(
                *static_cast<const Discrete*>(
                    target));
        }

        template<typename Discrete>
        static void writeDiscrete(
            void* target,
            int32_t value)
        {
            *static_cast<Discrete*>(target) =
                static_cast<Discrete>(value);
        }

        template<typename Integer>
        static bool fitsIntegerRange(
            Integer value,
            int32_t minimum,
            int32_t maximum)
        {
            const int64_t current =
                static_cast<int64_t>(value);

            if (current < minimum ||
                current > maximum)
            {
                return false;
            }

            if (std::numeric_limits<Integer>::is_signed)
            {
                return
                    static_cast<int64_t>(minimum) >=
                        static_cast<int64_t>(
                            std::numeric_limits<Integer>::lowest()) &&
                    static_cast<int64_t>(maximum) <=
                        static_cast<int64_t>(
                            std::numeric_limits<Integer>::max());
            }

            if (minimum < 0)
                return false;

            return static_cast<uint64_t>(maximum) <=
                   static_cast<uint64_t>(
                       std::numeric_limits<Integer>::max());
        }

        template<typename Discrete>
        static bool isDiscreteValueSupported(
            Discrete value)
        {
            using Storage =
                typename ParameterDiscreteStorage<
                    Discrete,
                    std::is_enum<Discrete>::value>::Type;

            const Storage storedValue =
                static_cast<Storage>(value);

            if (std::numeric_limits<Storage>::is_signed)
            {
                const int64_t signedValue =
                    static_cast<int64_t>(
                        storedValue);

                return signedValue >= INT32_MIN &&
                       signedValue <= INT32_MAX;
            }

            return static_cast<uint64_t>(
                       storedValue) <=
                   static_cast<uint64_t>(
                       INT32_MAX);
        }

        template<typename Discrete, size_t OptionCount>
        static bool doSelectionOptionsFit(
            const ParameterOption (&options)[OptionCount])
        {
            using Storage =
                typename ParameterDiscreteStorage<
                    Discrete,
                    std::is_enum<Discrete>::value>::Type;

            for (size_t i = 0;
                 i < OptionCount;
                 i++)
            {
                if (std::numeric_limits<Storage>::is_signed)
                {
                    if (static_cast<int64_t>(
                            options[i].value) <
                            static_cast<int64_t>(
                                std::numeric_limits<
                                    Storage>::lowest()) ||
                        static_cast<int64_t>(
                            options[i].value) >
                            static_cast<int64_t>(
                                std::numeric_limits<
                                    Storage>::max()))
                    {
                        return false;
                    }
                }
                else
                {
                    if (options[i].value < 0 ||
                        static_cast<uint64_t>(
                            options[i].value) >
                            static_cast<uint64_t>(
                                std::numeric_limits<
                                    Storage>::max()))
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    };

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
    bool hasError() const;

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

    Writer forOwner(
        const ParameterOwner& owner);

private:
    bool addBool(
        const ParameterOwner& owner,
        const char* key,
        const char* name,
        bool& value);

    bool addInteger(
        const ParameterOwner& owner,
        const char* key,
        const char* name,
        const ParameterDiscreteBinding& binding,
        int32_t minimum,
        int32_t maximum,
        int32_t step,
        const char* unit = nullptr);

    bool addDouble(
        const ParameterOwner& owner,
        const char* key,
        const char* name,
        double_t& value,
        double_t minimum,
        double_t maximum,
        double_t step,
        uint8_t decimals,
        const char* unit = nullptr);

    bool addSelection(
        const ParameterOwner& owner,
        const char* key,
        const char* name,
        const ParameterDiscreteBinding& binding,
        const ParameterOption* options,
        uint8_t optionCount);

    /**
     * @brief Crée la partie commune d'un Parameter.
     *
     * Retourne nullptr si :
     * - la liste n'est pas initialisée ;
     * - la liste est pleine ;
     * - une métadonnée ou une chaîne est invalide ;
     * - la paire ownerKey/key existe déjà.
     */
    Parameter* create(
        const ParameterOwner& owner,
        const char* key,
        const char* name,
        Parameter::Type type);

    static bool isValidText(const char* text);
    static bool isValidOwner(
        const ParameterOwner& owner);

    bool remember(bool result);

    Parameter* parameters = nullptr;

    size_t parameterCapacity = 0;
    size_t parameterCount = 0;
    size_t selectionOptionCount = 0;
    bool registrationError = false;
};

#endif
