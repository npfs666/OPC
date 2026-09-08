#include <Physics/Thermocouple.h>

#include <cstddef>

namespace Physics
{
namespace Thermocouple
{
namespace
{
    // Fonctions directes ITS-90, NIST SRD 60 / Monographie 175.
    // Source : https://its90.nist.gov/downloadFiles/type_k.tab.txt
    // Même chemin pour type_b, type_e, type_j, type_n, type_r, type_s, type_t.
    // Coefficients en mV et °C, du terme constant au degré le plus élevé.
    struct Polynomial
    {
        double minimumC;
        double maximumC;
        const double* coefficients;
        size_t count;
        bool exponential;
    };

    struct TypeData
    {
        Type type;
        const char* name;
        const Polynomial* polynomials;
        size_t count;
        double measurementMinimumC;
    };

    constexpr double B_0[] = {
        0.000000000000E+00,
        -0.246508183460E-03,
        0.590404211710E-05,
        -0.132579316360E-08,
        0.156682919010E-11,
        -0.169445292400E-14,
        0.629903470940E-18
    };

    constexpr double B_1[] = {
        -0.389381686210E+01,
        0.285717474700E-01,
        -0.848851047850E-04,
        0.157852801640E-06,
        -0.168353448640E-09,
        0.111097940130E-12,
        -0.445154310330E-16,
        0.989756408210E-20,
        -0.937913302890E-24
    };

    constexpr Polynomial B_POLYNOMIALS[] = {
        {0, 630.615, B_0, 7, false},
        {630.615, 1820, B_1, 9, false}
    };

    constexpr double E_0[] = {
        0.000000000000E+00,
        0.586655087080E-01,
        0.454109771240E-04,
        -0.779980486860E-06,
        -0.258001608430E-07,
        -0.594525830570E-09,
        -0.932140586670E-11,
        -0.102876055340E-12,
        -0.803701236210E-15,
        -0.439794973910E-17,
        -0.164147763550E-19,
        -0.396736195160E-22,
        -0.558273287210E-25,
        -0.346578420130E-28
    };

    constexpr double E_1[] = {
        0.000000000000E+00,
        0.586655087100E-01,
        0.450322755820E-04,
        0.289084072120E-07,
        -0.330568966520E-09,
        0.650244032700E-12,
        -0.191974955040E-15,
        -0.125366004970E-17,
        0.214892175690E-20,
        -0.143880417820E-23,
        0.359608994810E-27
    };

    constexpr Polynomial E_POLYNOMIALS[] = {
        {-270, 0, E_0, 14, false},
        {0, 1000, E_1, 11, false}
    };

    constexpr double J_0[] = {
        0.000000000000E+00,
        0.503811878150E-01,
        0.304758369300E-04,
        -0.856810657200E-07,
        0.132281952950E-09,
        -0.170529583370E-12,
        0.209480906970E-15,
        -0.125383953360E-18,
        0.156317256970E-22
    };

    constexpr double J_1[] = {
        0.296456256810E+03,
        -0.149761277860E+01,
        0.317871039240E-02,
        -0.318476867010E-05,
        0.157208190040E-08,
        -0.306913690560E-12
    };

    constexpr Polynomial J_POLYNOMIALS[] = {
        {-210, 760, J_0, 9, false},
        {760, 1200, J_1, 6, false}
    };

    constexpr double K_0[] = {
        0.000000000000E+00,
        0.394501280250E-01,
        0.236223735980E-04,
        -0.328589067840E-06,
        -0.499048287770E-08,
        -0.675090591730E-10,
        -0.574103274280E-12,
        -0.310888728940E-14,
        -0.104516093650E-16,
        -0.198892668780E-19,
        -0.163226974860E-22
    };

    constexpr double K_1[] = {
        -0.176004136860E-01,
        0.389212049750E-01,
        0.185587700320E-04,
        -0.994575928740E-07,
        0.318409457190E-09,
        -0.560728448890E-12,
        0.560750590590E-15,
        -0.320207200030E-18,
        0.971511471520E-22,
        -0.121047212750E-25
    };

    constexpr Polynomial K_POLYNOMIALS[] = {
        {-270, 0, K_0, 11, false},
        {0, 1372, K_1, 10, true}
    };

    constexpr double N_0[] = {
        0.000000000000E+00,
        0.261591059620E-01,
        0.109574842280E-04,
        -0.938411115540E-07,
        -0.464120397590E-10,
        -0.263033577160E-11,
        -0.226534380030E-13,
        -0.760893007910E-16,
        -0.934196678350E-19
    };

    constexpr double N_1[] = {
        0.000000000000E+00,
        0.259293946010E-01,
        0.157101418800E-04,
        0.438256272370E-07,
        -0.252611697940E-09,
        0.643118193390E-12,
        -0.100634715190E-14,
        0.997453389920E-18,
        -0.608632456070E-21,
        0.208492293390E-24,
        -0.306821961510E-28
    };

    constexpr Polynomial N_POLYNOMIALS[] = {
        {-270, 0, N_0, 9, false},
        {0, 1300, N_1, 11, false}
    };

    constexpr double R_0[] = {
        0.000000000000E+00,
        0.528961729765E-02,
        0.139166589782E-04,
        -0.238855693017E-07,
        0.356916001063E-10,
        -0.462347666298E-13,
        0.500777441034E-16,
        -0.373105886191E-19,
        0.157716482367E-22,
        -0.281038625251E-26
    };

    constexpr double R_1[] = {
        0.295157925316E+01,
        -0.252061251332E-02,
        0.159564501865E-04,
        -0.764085947576E-08,
        0.205305291024E-11,
        -0.293359668173E-15
    };

    constexpr double R_2[] = {
        0.152232118209E+03,
        -0.268819888545E+00,
        0.171280280471E-03,
        -0.345895706453E-07,
        -0.934633971046E-14
    };

    constexpr Polynomial R_POLYNOMIALS[] = {
        {-50, 1064.18, R_0, 10, false},
        {1064.18, 1664.5, R_1, 6, false},
        {1664.5, 1768.1, R_2, 5, false}
    };

    constexpr double S_0[] = {
        0.000000000000E+00,
        0.540313308631E-02,
        0.125934289740E-04,
        -0.232477968689E-07,
        0.322028823036E-10,
        -0.331465196389E-13,
        0.255744251786E-16,
        -0.125068871393E-19,
        0.271443176145E-23
    };

    constexpr double S_1[] = {
        0.132900444085E+01,
        0.334509311344E-02,
        0.654805192818E-05,
        -0.164856259209E-08,
        0.129989605174E-13
    };

    constexpr double S_2[] = {
        0.146628232636E+03,
        -0.258430516752E+00,
        0.163693574641E-03,
        -0.330439046987E-07,
        -0.943223690612E-14
    };

    constexpr Polynomial S_POLYNOMIALS[] = {
        {-50, 1064.18, S_0, 9, false},
        {1064.18, 1664.5, S_1, 5, false},
        {1664.5, 1768.1, S_2, 5, false}
    };

    constexpr double T_0[] = {
        0.000000000000E+00,
        0.387481063640E-01,
        0.441944343470E-04,
        0.118443231050E-06,
        0.200329735540E-07,
        0.901380195590E-09,
        0.226511565930E-10,
        0.360711542050E-12,
        0.384939398830E-14,
        0.282135219250E-16,
        0.142515947790E-18,
        0.487686622860E-21,
        0.107955392700E-23,
        0.139450270620E-26,
        0.797951539270E-30
    };

    constexpr double T_1[] = {
        0.000000000000E+00,
        0.387481063640E-01,
        0.332922278800E-04,
        0.206182434040E-06,
        -0.218822568460E-08,
        0.109968809280E-10,
        -0.308157587720E-13,
        0.454791352900E-16,
        -0.275129016730E-19
    };

    constexpr Polynomial T_POLYNOMIALS[] = {
        {-270, 0, T_0, 15, false},
        {0, 400, T_1, 9, false}
    };

    constexpr TypeData TYPES[] = {
        {Type::B, "B", B_POLYNOMIALS, 2, 250},
        {Type::E, "E", E_POLYNOMIALS, 2, -270},
        {Type::J, "J", J_POLYNOMIALS, 2, -210},
        {Type::K, "K", K_POLYNOMIALS, 2, -270},
        {Type::N, "N", N_POLYNOMIALS, 2, -270},
        {Type::R, "R", R_POLYNOMIALS, 3, -50},
        {Type::S, "S", S_POLYNOMIALS, 3, -50},
        {Type::T, "T", T_POLYNOMIALS, 2, -270}
    };

    const TypeData* findType(Type type)
    {
        for (const TypeData& data : TYPES)
        {
            if (data.type == type)
                return &data;
        }
        return nullptr;
    }

    double evaluate(const TypeData& data, double temperatureC)
    {
        if (!std::isfinite(temperatureC))
            return NAN;

        for (size_t i = 0; i < data.count; i++)
        {
            const Polynomial& polynomial = data.polynomials[i];
            if (temperatureC < polynomial.minimumC ||
                temperatureC > polynomial.maximumC)
            {
                continue;
            }

            // Schéma de Horner : pas de pow() ni d'allocation dynamique.
            double voltageMv = polynomial.coefficients[polynomial.count - 1];
            for (size_t j = polynomial.count - 1; j > 0; j--)
                voltageMv = voltageMv * temperatureC + polynomial.coefficients[j - 1];

            if (polynomial.exponential)
            {
                // Terme supplémentaire de la fonction type K au-dessus de 0 °C.
                const double delta = temperatureC - 0.126968600000E+03;
                voltageMv += 0.118597600000E+00 *
                    std::exp(-0.118343200000E-03 * delta * delta);
            }
            return voltageMv;
        }
        return NAN;
    }
}

const char* typeName(Type type)
{
    const TypeData* data = findType(type);
    return data != nullptr ? data->name : "?";
}

Range referenceRange(Type type)
{
    const TypeData* data = findType(type);
    if (data == nullptr)
        return {NAN, NAN};

    return {
        data->polynomials[0].minimumC,
        data->polynomials[data->count - 1].maximumC
    };
}

Range measurementRange(Type type)
{
    const TypeData* data = findType(type);
    if (data == nullptr)
        return {NAN, NAN};

    return {
        data->measurementMinimumC,
        data->polynomials[data->count - 1].maximumC
    };
}

double temperatureToMillivolts(Type type, double temperatureC)
{
    const TypeData* data = findType(type);
    return data != nullptr ? evaluate(*data, temperatureC) : NAN;
}

double millivoltsToTemperature(Type type, double voltageMv)
{
    const TypeData* data = findType(type);
    if (data == nullptr || !std::isfinite(voltageMv))
        return NAN;

    double lowerC = data->measurementMinimumC;
    double upperC = data->polynomials[data->count - 1].maximumC;
    const double lowerMv = evaluate(*data, lowerC);
    const double upperMv = evaluate(*data, upperC);

    if (voltageMv < lowerMv || voltageMv > upperMv)
        return NAN;
    if (voltageMv == lowerMv)
        return lowerC;
    if (voltageMv == upperMv)
        return upperC;

    // Inversion bornée de la même fonction directe sur son domaine croissant.
    // 32 étapes ramènent l'intervalle sous 0,000001 °C. Cela évite de dupliquer
    // des polynômes inverses dont certains ne couvrent qu'une partie du domaine.
    // B commence à 250 °C ; sa courbe n'est pas monotone près de 0 °C.
    for (uint8_t i = 0; i < 32; i++)
    {
        const double middleC = (lowerC + upperC) * 0.5;
        if (evaluate(*data, middleC) < voltageMv)
            lowerC = middleC;
        else
            upperC = middleC;
    }
    return (lowerC + upperC) * 0.5;
}

double compensatedTemperature(
    Type type,
    double voltageMv,
    double coldJunctionC)
{
    if (!std::isfinite(voltageMv))
        return NAN;

    const double coldJunctionMv = temperatureToMillivolts(type, coldJunctionC);
    if (!std::isfinite(coldJunctionMv))
        return NAN;

    return millivoltsToTemperature(type, voltageMv + coldJunctionMv);
}
}
}

