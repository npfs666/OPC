#include <hmi/HomeSetpointEditor.h>

#include <cstdlib>

void HomeSetpointEditor::begin(
    double_t value,
    double_t minimum,
    double_t maximum,
    double_t step,
    uint32_t now)
{
    this->minimum = minimum;
    this->maximum = maximum;
    this->step = step > 0.0 ? step : 0.1;

    initialValue = value;
    editedValue = value;
    lastActivity = now;
    hasDetent = false;
    active = true;
}

void HomeSetpointEditor::rotate(int32_t detents, uint32_t now)
{
    if (!active || detents == 0)
        return;

    const bool fast =
        std::abs(detents) > 1 ||
        (hasDetent && now - lastDetent < FAST_DETENT_MS);

    const double_t target =
        editedValue +
        detents * (fast ? FAST_FACTOR : 1) * step;

    // Grille du pas : évite la dérive des additions successives.
    double_t snapped = std::round(target / step) * step;

    if (snapped < minimum)
        snapped = minimum;
    else if (snapped > maximum)
        snapped = maximum;

    editedValue = snapped;
    lastActivity = now;
    lastDetent = now;
    hasDetent = true;
}
