#include <Outputs/ThreePointActuator.h>

#include <Outputs/Output.h>
#include <Regulator/Regulator.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>

namespace
{
    constexpr uint32_t MIN_TRAVEL_TIME = 10;
    constexpr uint32_t MAX_TRAVEL_TIME = 600;

    // Commande considérée comme une butée (fermeture ou ouverture franche).
    constexpr double_t BOUND = 0.001;

    // Sens stable avant de mettre en marche (temps de collage du relais).
    constexpr uint32_t DIRECTION_SETTLE_MS = 100;

    bool isBinary(double_t command)
    {
        return command <= BOUND || command >= 1.0 - BOUND;
    }
}

ThreePointActuator::ThreePointActuator()
{
}

void ThreePointActuator::begin(
    const char* name,
    Regulator& regulator,
    Output& open,
    Output& close,
    uint32_t travelTime)
{
    begin(name, name, regulator, open, close, travelTime);
}

void ThreePointActuator::begin(
    const char* key,
    const char* name,
    Regulator& regulator,
    Output& open,
    Output& close,
    uint32_t travelTime)
{
    setup(key, name, regulator, Wiring::OpenClose, open, close, travelTime);
}

void ThreePointActuator::beginRunDirection(
    const char* name,
    Regulator& regulator,
    Output& run,
    Output& direction,
    uint32_t travelTime)
{
    beginRunDirection(name, name, regulator, run, direction, travelTime);
}

void ThreePointActuator::beginRunDirection(
    const char* key,
    const char* name,
    Regulator& regulator,
    Output& run,
    Output& direction,
    uint32_t travelTime)
{
    setup(
        key,
        name,
        regulator,
        Wiring::RunDirection,
        run,
        direction,
        travelTime);
}

void ThreePointActuator::setup(
    const char* key,
    const char* name,
    Regulator& regulator,
    Wiring wiring,
    Output& first,
    Output& second,
    uint32_t travelTime)
{
    Actuator::begin(key, name, regulator);

    wiringMode = wiring;
    openOrRun = &first;
    closeOrDirection = &second;

    settings = Settings{};
    settings.travelTime =
        travelTime < MIN_TRAVEL_TIME
            ? MIN_TRAVEL_TIME
            : travelTime > MAX_TRAVEL_TIME
                ? MAX_TRAVEL_TIME
                : travelTime;

    estimate = 0.0;
    beyondMs = 0.0;
    continuousMs = 0.0;
    known = false;
    calibration = Motion::Stopped;
    movement = Motion::Stopped;
    previousApplied = Motion::Stopped;
    lastMotion = Motion::Stopped;
    previousDirection = false;
    tracking = false;
}

bool ThreePointActuator::acceptOutput(Output& output)
{
    if (outputCount >= 2 ||
        (&output != openOrRun && &output != closeOrDirection))
    {
        return false;
    }

    // Verrouillé ici plutôt que dans begin() : le begin() d'une sortie PWM
    // lève son verrou.
    if (&output == lockedOutput())
        output.lockSafeCommand(0.0);

    return true;
}

Output* ThreePointActuator::lockedOutput() const
{
    return wiringMode == Wiring::OpenClose
        ? openOrRun
        : closeOrDirection;
}

Output* ThreePointActuator::fallbackOutput() const
{
    return wiringMode == Wiring::OpenClose
        ? closeOrDirection
        : openOrRun;
}

double_t ThreePointActuator::travelMs() const
{
    return settings.travelTime * 1000.0;
}

double_t ThreePointActuator::overtravelMs() const
{
    return travelMs() * settings.overtravel / 100.0;
}

bool ThreePointActuator::isOn(const Output* output)
{
    return output != nullptr && output->appliedCommand() >= 0.5;
}

ThreePointActuator::Motion ThreePointActuator::appliedMotion() const
{
    const bool first = isOn(openOrRun);
    const bool second = isOn(closeOrDirection);

    if (wiringMode == Wiring::RunDirection)
    {
        if (!first)
            return Motion::Stopped;

        return second ? Motion::Opening : Motion::Closing;
    }

    // Les deux sens alimentés : jamais commandé, traité comme un arrêt.
    if (first == second)
        return Motion::Stopped;

    return first ? Motion::Opening : Motion::Closing;
}

void ThreePointActuator::track(uint32_t now)
{
    const Motion applied = appliedMotion();
    const bool direction = isOn(closeOrDirection);

    if (!tracking)
    {
        tracking = true;
        lastTrack = now;
        stoppedAt = now;
        directionChangedAt = now;
        previousApplied = applied;
        previousDirection = direction;

        // Repli en marche au démarrage : la pause d'inversion s'applique.
        if (applied != Motion::Stopped)
            lastMotion = applied;

        return;
    }

    const double_t elapsedMs = static_cast<double_t>(now - lastTrack);
    lastTrack = now;

    if (applied != Motion::Stopped)
    {
        const double_t delta =
            (applied == Motion::Opening ? elapsedMs : -elapsedMs) /
            travelMs();

        const double_t next = estimate + delta;

        if (next >= 1.0)
        {
            beyondMs += (next - 1.0) * travelMs();
            estimate = 1.0;
        }
        else if (next <= 0.0)
        {
            beyondMs += -next * travelMs();
            estimate = 0.0;
        }
        else
        {
            estimate = next;
            beyondMs = 0.0;
        }
    }

    // Le début exact d'une marche est inconnu : seule la marche observée
    // sur deux appels consécutifs compte.
    continuousMs =
        applied != Motion::Stopped && applied == previousApplied
            ? continuousMs + elapsedMs
            : 0.0;

    if (!known &&
        applied != Motion::Stopped &&
        continuousMs >= travelMs() + overtravelMs())
    {
        known = true;
        estimate = applied == Motion::Opening ? 1.0 : 0.0;
        beyondMs = overtravelMs();
    }

    // Arrêt daté à sa première observation, donc au plus tôt : la pause
    // d'inversion n'est jamais raccourcie.
    if (applied == Motion::Stopped && previousApplied != Motion::Stopped)
        stoppedAt = now;

    if (applied != Motion::Stopped)
        lastMotion = applied;

    if (direction != previousDirection)
        directionChangedAt = now;

    previousApplied = applied;
    previousDirection = direction;
}

ThreePointActuator::Motion ThreePointActuator::wantedMotion(
    double_t target)
{
    if (!known)
    {
        if (calibration == Motion::Stopped)
        {
            calibration =
                target >= 0.5
                    ? Motion::Opening
                    : Motion::Closing;
        }

        return calibration;
    }

    calibration = Motion::Stopped;

    // Fermeture ou ouverture franche : jusqu'en butée, puis la sur-course,
    // qui recale l'estimation.
    if (target <= BOUND)
    {
        return estimate <= 0.0 && beyondMs >= overtravelMs()
            ? Motion::Stopped
            : Motion::Closing;
    }

    if (target >= 1.0 - BOUND)
    {
        return estimate >= 1.0 && beyondMs >= overtravelMs()
            ? Motion::Stopped
            : Motion::Opening;
    }

    // Une marche lancée va jusqu'à la consigne ; une nouvelle marche ne
    // part qu'au-delà de la zone morte.
    if (movement == Motion::Opening && estimate < target)
        return Motion::Opening;

    if (movement == Motion::Closing && estimate > target)
        return Motion::Closing;

    const double_t band = settings.deadband / 100.0;

    if (target - estimate > band)
        return Motion::Opening;

    if (estimate - target > band)
        return Motion::Closing;

    return Motion::Stopped;
}

void ThreePointActuator::drive(Motion wanted, uint32_t now)
{
    movement = wanted;

    const Motion applied = appliedMotion();

    const bool reversal =
        lastMotion != Motion::Stopped &&
        wanted != lastMotion;

    const bool mayStart =
        applied == Motion::Stopped &&
        (!reversal || now - stoppedAt >= settings.reversalPause);

    bool first = false;
    bool second = false;

    if (wiringMode == Wiring::OpenClose)
    {
        // Un sens ne part que les deux sorties coupées : jamais les deux
        // alimentées, même avec les temps minimaux d'un relais.
        first =
            wanted == Motion::Opening &&
            (applied == Motion::Opening || mayStart);

        second =
            wanted == Motion::Closing &&
            (applied == Motion::Closing || mayStart);
    }
    else
    {
        const bool direction = isOn(closeOrDirection);
        const bool wantedDirection = wanted == Motion::Opening;

        second = direction;

        if (wanted != Motion::Stopped)
        {
            if (applied == wanted)
            {
                first = true;
            }
            else if (mayStart)
            {
                // Sens basculé Marche coupée, puis marche une fois le
                // relais Sens collé.
                if (direction != wantedDirection)
                    second = wantedDirection;
                else
                    first = now - directionChangedAt >= DIRECTION_SETTLE_MS;
            }
        }
    }

    openOrRun->setCommand(first ? 1.0 : 0.0, now);
    closeOrDirection->setCommand(second ? 1.0 : 0.0, now);
}

void ThreePointActuator::forceSafeOutputs()
{
    Output* const pair[] = {openOrRun, closeOrDirection};

    // Coupure avant mise en marche, comme ProcessControl::forceSafeOutputs().
    for (uint8_t pass = 0; pass < 2; pass++)
    {
        for (Output* output : pair)
        {
            if (output != nullptr &&
                (output->safeCommand() > 0.0) == (pass == 1))
            {
                output->forceSafe();
            }
        }
    }
}

void ThreePointActuator::update(uint32_t now)
{
    track(now);

    const bool commandValid =
        regulator != nullptr &&
        regulator->isCommandValid() &&
        std::isfinite(regulator->readCommand());

    if (!commandValid || outputCount != 2)
    {
        movement = Motion::Stopped;
        forceSafeOutputs();
        return;
    }

    double_t target = regulator->readCommand();

    if (target < 0.0)
        target = 0.0;
    else if (target > 1.0)
        target = 1.0;

    drive(wantedMotion(target), now);
}

void ThreePointActuator::resume(uint32_t now)
{
    track(now);
    movement = Motion::Stopped;
}

double_t ThreePointActuator::position() const
{
    return estimate;
}

bool ThreePointActuator::isPositionKnown() const
{
    return known;
}

ThreePointActuator::Wiring ThreePointActuator::wiring() const
{
    return wiringMode;
}

double_t ThreePointActuator::printValue() const
{
    return estimate;
}

void ThreePointActuator::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "actuators",
        "Actionneurs",
        getConfigurationKey(),
        getName()
    });

    parameters.addInteger(
        "travel_time",
        "Temps de course",
        settings.travelTime,
        MIN_TRAVEL_TIME,
        MAX_TRAVEL_TIME,
        5,
        "s");

    parameters.addDouble(
        "deadband",
        "Zone morte",
        settings.deadband,
        0.5,
        10.0,
        0.5,
        1,
        "%");

    parameters.addInteger(
        "reversal_pause",
        "Pause inversion",
        settings.reversalPause,
        100,
        5000,
        100,
        "ms");

    parameters.addInteger(
        "overtravel",
        "Sur-course",
        settings.overtravel,
        0,
        100,
        5,
        "%");

    Actuator::registerParameters(list);
}

bool ThreePointActuator::validateParameters(
    const ParameterEditor& editor) const
{
    const Output* fallback = fallbackOutput();

    if (fallback == nullptr)
        return true;

    // Relais : état sûr booléen, toujours valable. Sortie PWM : 0 ou 1.
    const ParameterDraft* safeCommand =
        editor.find(fallback->configurationKey(), "safe_command");

    if (safeCommand == nullptr ||
        safeCommand->parameter == nullptr ||
        safeCommand->parameter->type != Parameter::Type::Double)
    {
        return true;
    }

    return isBinary(safeCommand->numberValue);
}
