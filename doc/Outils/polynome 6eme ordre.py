def pt100_resistance_to_temp_poly6(r_measured):
    """
    Calcule la température (°C) d'une sonde Pt100 à partir de sa résistance (Ohm)
    en utilisant un polynôme d'ordre 6 optimisé (méthode de Horner).
    Garantit une excellente précision sur la plage -200 °C à +850 °C.
    """
    # Coefficients du polynôme d'ordre 6
    p0 = -2.421287e02
    p1 =  2.221232e00
    p2 =  2.965978e-03
    p3 = -1.402882e-05
    p4 =  5.187559e-08
    p5 = -9.211748e-11
    p6 =  6.715985e-14

    # Application de la méthode de Horner pour la stabilité numérique et la vitesse
    temperature = p0 + r_measured * (p1 + r_measured * (p2 + r_measured * (p3 + r_measured * (p4 + r_measured * (p5 + r_measured * p6)))))

    return temperature




def pt100_resistance_to_temp_newton(r_measured, tolerance=1e-5, max_iter=20):
    """
    Calcule la température (°C) d'une sonde Pt100 (IEC 60751) à partir de sa résistance
    par la méthode itérative de Newton-Raphson pour une précision maximale.
    """
    # Constantes IEC 60751
    R0 = 100.0
    A = 3.9083e-3
    B = -5.775e-7
    C = -4.183e-12

    # Estimation initiale de la température (approximation linéaire)
    t = (r_measured - R0) / (R0 * A)

    for _ in range(max_iter):
        if t >= 0:
            f = R0 * (1.0 + A * t + B * t**2) - r_measured
            f_prime = R0 * (A + 2.0 * B * t)
        else:
            f = R0 * (1.0 + A * t + B * t**2 + C * (t - 100.0) * t**3) - r_measured
            f_prime = R0 * (A + 2.0 * B * t - 300.0 * C * t**2 + 4.0 * C * t**3)

        delta = f / f_prime
        t -= delta

        if abs(delta) < tolerance:
            break

    return t


import math

def get_resistance_to_temperature(resistance: float) -> float:
    """
    Conversion d'une résistance en température via le calcul par interpolation.
    """
    # Table d'interpolation (chaque ligne correspond à un pas de +10 Ohms)
    interpolation_table = [
        -500.000, -219.415, -196.509, -173.118, -149.304, -125.122, -100.617, -75.827, -50.781, -25.501,
         0.000,   25.686,   51.571,   77.660,  103.958,  130.469,  157.198,  184.152,  211.336,  238.756,
         266.419
    ]

    interpolation_size = len(interpolation_table)
    resistance_step = 10.0
    minimum_resistance = resistance_step
    maximum_resistance = (interpolation_size - 1) * resistance_step

    # Vérification des bornes et de la validité de la valeur
    if not math.isfinite(resistance) or resistance < minimum_resistance or resistance > maximum_resistance:
        return math.nan

    table_position = resistance / resistance_step
    index = int(table_position)
    fraction = table_position - index

    # Si la valeur tombe pile sur un index ou si on atteint la fin du tableau
    if fraction == 0.0 or index >= interpolation_size - 1:
        return interpolation_table[index]

    # Première plage : Interpolation linéaire
    if index == 1:
        lower = interpolation_table[index]
        upper = interpolation_table[index + 1]
        return lower + fraction * (upper - lower)

    # Autres plages : Interpolation quadratique à trois points
    previous = interpolation_table[index - 1]
    current = interpolation_table[index]
    next = interpolation_table[index + 1]

    return current + 0.5 * fraction * (next - previous + fraction * (previous - 2.0 * current + next))




#print(pt100_resistance_to_temp_poly6(100.061))
print(pt100_resistance_to_temp_newton(100.061))
print(get_resistance_to_temperature(100.061))


#print(pt100_resistance_to_temp_poly6(108))
print(pt100_resistance_to_temp_newton(108))
print(get_resistance_to_temperature(108))

print(pt100_resistance_to_temp_newton(120.027))
print(get_resistance_to_temperature(120.027))


