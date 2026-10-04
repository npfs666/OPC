#ifndef EVENT_LOG_SCREEN_H
#define EVENT_LOG_SCREEN_H

#include <cstddef>
#include <cstdint>

class Adafruit_GFX;
class EventLog;
struct EventEntry;

/**
 * Visionneuse du journal (menu Divers > Journal), en taille 2 comme le
 * menu, du plus récent au plus ancien : pour chaque événement, la date en
 * gris puis le texte, en couleur selon le type (défaut rouge, alarme orange,
 * redémarrage cyan). Chaque événement occupe la hauteur de son texte ;
 * l'écran en montre autant qu'il en tient.
 */
namespace EventLogScreen
{
    // Taille 2 : 19 caractères par ligne, 18 px par ligne.
    constexpr size_t LINE_CHARS = 19;
    constexpr size_t MAX_TEXT_LINES = 3;

    /** Dernier indice pouvant être en haut de l'écran. */
    size_t lastFirstIndex(const EventLog& log);

    /** Couleur RGB565 du texte d'un événement. */
    uint16_t color(const EventEntry& entry);

    /** "04/10 14:32", ou "+42 s" si l'heure était inconnue. */
    size_t formatTime(
        const EventEntry& entry,
        char* buffer,
        size_t size);

    /**
     * Coupe text (CP437) en lignes d'au plus LINE_CHARS caractères, de
     * préférence sur une espace. Un texte trop long se termine par "..".
     * @return Nombre de lignes (au plus MAX_TEXT_LINES)
     */
    size_t wrap(
        const char* text,
        char lines[MAX_TEXT_LINES][LINE_CHARS + 1]);

    void draw(
        Adafruit_GFX& display,
        const EventLog& log,
        size_t first);
}

#endif
