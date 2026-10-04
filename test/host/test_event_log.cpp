#include "TestHarness.h"

#include <EventLog.h>
#include <ProcessControl.h>
#include <hmi/EventLogScreen.h>

#include <cstring>

namespace
{
    ClockSample clockAt(uint8_t hour, uint8_t minute, uint8_t second)
    {
        ClockSample clock;
        clock.dateTime.year = 2026;
        clock.dateTime.month = 10;
        clock.dateTime.day = 4;
        clock.dateTime.hour = hour;
        clock.dateTime.minute = minute;
        clock.dateTime.second = second;
        clock.valid = true;
        return clock;
    }

    void testOrderAndCapacity()
    {
        EventLog log;
        const ClockSample clock = clockAt(14, 32, 5);
        CHECK_TRUE(log.count() == 0);
        CHECK_TRUE(log.at(0) == nullptr);

        char text[16];

        // Textes distincts, espacés : aucun regroupement.
        for (uint32_t i = 0; i < EventLog::CAPACITY + 10; i++)
        {
            std::snprintf(text, sizeof(text), "evt %u", static_cast<unsigned>(i));
            log.add(i * 100000, clock, EventKind::Info, false, text);
        }

        CHECK_TRUE(log.count() == EventLog::CAPACITY);
        CHECK_TRUE(std::strcmp(log.at(0)->text, "evt 73") == 0);
        CHECK_TRUE(std::strcmp(log.at(EventLog::CAPACITY - 1)->text, "evt 10") == 0);
        CHECK_TRUE(log.at(EventLog::CAPACITY) == nullptr);
    }

    void testCoalescing()
    {
        EventLog log;
        const ClockSample clock = clockAt(8, 0, 0);

        // Sonde qui bagote : deux textes alternés regroupés.
        for (uint32_t i = 0; i < 10; i++)
        {
            log.add(1000 + i * 100, clock, EventKind::Fault, true,
                    i % 2 == 0 ? "T : RUPTURE" : "T : OK");
        }

        CHECK_TRUE(log.count() == 2);
        CHECK_TRUE(log.at(0)->repeats == 5);
        CHECK_TRUE(log.at(1)->repeats == 5);

        // Hors de la fenêtre de regroupement : nouvelle ligne.
        log.add(1000 + EventLog::COALESCE_WINDOW_MS + 2000, clock,
                EventKind::Fault, true, "T : RUPTURE");
        CHECK_TRUE(log.count() == 3);

        // Même texte, type différent : nouvelle ligne.
        log.add(200000, clock, EventKind::Info, true, "T : RUPTURE");
        CHECK_TRUE(log.count() == 4);

        // Trop loin dans l'historique (plus de 4 événements) : nouvelle ligne.
        EventLog deep;
        deep.add(0, clock, EventKind::Info, false, "A");
        deep.add(1, clock, EventKind::Info, false, "B");
        deep.add(2, clock, EventKind::Info, false, "C");
        deep.add(3, clock, EventKind::Info, false, "D");
        deep.add(4, clock, EventKind::Info, false, "E");
        deep.add(5, clock, EventKind::Info, false, "A");
        CHECK_TRUE(deep.count() == 6);
    }

    void testPrintAndSaveFlags()
    {
        EventLog log;
        const ClockSample clock = clockAt(9, 15, 0);
        Stream stream;

        log.add(0, clock, EventKind::Info, false, "Réglages modifiés");
        CHECK_FALSE(log.hasUnsavedImportant());

        log.add(10, clock, EventKind::Fault, true, "T : RUPTURE");
        CHECK_TRUE(log.hasUnsavedImportant());

        log.printNew(stream);
        CHECK_TRUE(stream.printedLineCount == 2);

        // Répétition : pas de nouvelle ligne série, mais à sauvegarder.
        log.markSaved();
        log.add(20, clock, EventKind::Fault, true, "T : RUPTURE");
        log.printNew(stream);
        CHECK_TRUE(stream.printedLineCount == 2);
        CHECK_TRUE(log.hasUnsavedImportant());

        // Seuls les importants sont copiés, du plus ancien au plus récent.
        log.add(30, clock, EventKind::Alarm, true, "Alarme X : ACTIVE");
        EventEntry copies[EventLog::CAPACITY];
        const size_t copied = log.copyImportant(copies, EventLog::CAPACITY);
        CHECK_TRUE(copied == 2);
        CHECK_TRUE(std::strcmp(copies[0].text, "T : RUPTURE") == 0);
        CHECK_TRUE(copies[0].repeats == 2);
        CHECK_TRUE(std::strcmp(copies[1].text, "Alarme X : ACTIVE") == 0);
    }

    void testFormats()
    {
        EventLog log;
        log.add(125000, clockAt(14, 32, 5), EventKind::Fault, true, "T : RUPTURE");
        log.add(125500, clockAt(14, 32, 5), EventKind::Fault, true, "T : RUPTURE");

        char text[EventLog::LINE_SIZE];

        EventLog::formatTimestamp(*log.at(0), text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "04/10 14:32:05") == 0);

        EventLog::formatLine(*log.at(0), text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "04/10 14:32:05 T : RUPTURE (x2)") == 0);

        // Heure inconnue : temps depuis le démarrage.
        EventLog unknown;
        unknown.add(42000, ClockSample{}, EventKind::Restart, true, "Mise sous tension");
        EventLog::formatTimestamp(*unknown.at(0), text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "+42 s") == 0);
    }

    void testCsvRoundTrip()
    {
        EventLog log;
        log.add(125000, clockAt(14, 32, 5), EventKind::Alarm, true,
                "Alarme A;B : ACTIVE");
        log.add(125100, clockAt(14, 32, 5), EventKind::Alarm, true,
                "Alarme A;B : ACTIVE");

        char line[EventLog::LINE_SIZE];
        EventLog::formatCsv(*log.at(0), line, sizeof(line));
        CHECK_TRUE(std::strcmp(line,
                   "2026-10-04 14:32:05;125100;2;2;Alarme A;B : ACTIVE") == 0);

        EventEntry parsed;
        CHECK_TRUE(EventLog::parseCsv(line, parsed));
        CHECK_TRUE(parsed.timeValid);
        CHECK_TRUE(parsed.time.year == 2026 && parsed.time.second == 5);
        CHECK_TRUE(parsed.kind == EventKind::Alarm);
        CHECK_TRUE(parsed.repeats == 2);
        CHECK_TRUE(parsed.important);
        CHECK_TRUE(std::strcmp(parsed.text, "Alarme A;B : ACTIVE") == 0);

        // Heure inconnue et fin de ligne Windows.
        CHECK_TRUE(EventLog::parseCsv("-;42000;3;1;Mise sous tension\r", parsed));
        CHECK_FALSE(parsed.timeValid);
        CHECK_TRUE(parsed.kind == EventKind::Restart);
        CHECK_TRUE(std::strcmp(parsed.text, "Mise sous tension") == 0);

        // Lignes invalides.
        CHECK_FALSE(EventLog::parseCsv("pas de champs", parsed));
        CHECK_FALSE(EventLog::parseCsv("2026-10-04 14:32:05;1;9;1;type inconnu", parsed));
        CHECK_FALSE(EventLog::parseCsv("hier;1;0;1;date invalide", parsed));

        // Relecture : événements anciens, importants, déjà sauvegardés.
        EventLog restored;
        CHECK_TRUE(EventLog::parseCsv(line, parsed));
        restored.restore(parsed);
        CHECK_TRUE(restored.count() == 1);
        CHECK_FALSE(restored.hasUnsavedImportant());
        Stream stream;
        restored.printNew(stream);
        CHECK_TRUE(stream.printedLineCount == 0);
    }

    void testProcessEvents()
    {
        ProcessControl process;
        process.updateClock(clockAt(10, 0, 0));
        process.logEvent(5000, EventKind::Info, true, "RAZ compteurs %s", "Relais 1");

        const EventEntry* entry = process.eventLog().at(0);
        CHECK_TRUE(entry != nullptr);
        if (entry == nullptr)
            return;

        CHECK_TRUE(std::strcmp(entry->text, "RAZ compteurs Relais 1") == 0);
        CHECK_TRUE(entry->timeValid && entry->time.hour == 10);

        // Texte trop long : tronqué à la taille d'un événement.
        process.logEvent(6000, EventKind::Info, false, "%s",
                         "0123456789012345678901234567890123456789012345678901234567890");
        CHECK_TRUE(std::strlen(process.eventLog().at(0)->text) ==
                   EventLog::TEXT_SIZE - 1);
    }

    void testScreenHelpers()
    {
        char lines[EventLogScreen::MAX_TEXT_LINES][EventLogScreen::LINE_CHARS + 1];

        CHECK_TRUE(EventLogScreen::wrap("Court", lines) == 1);
        CHECK_TRUE(std::strcmp(lines[0], "Court") == 0);

        // Coupé sur les espaces, 19 caractères par ligne au plus.
        CHECK_TRUE(EventLogScreen::wrap(
                       "Alarme Temp. haute : MEMORISEE", lines) == 2);
        CHECK_TRUE(std::strcmp(lines[0], "Alarme Temp. haute") == 0);
        CHECK_TRUE(std::strcmp(lines[1], ": MEMORISEE") == 0);

        // Mot plus long qu'une ligne : coupe nette.
        CHECK_TRUE(EventLogScreen::wrap(
                       "Temperaturethermostatpid : OK", lines) == 2);
        CHECK_TRUE(std::strlen(lines[0]) == EventLogScreen::LINE_CHARS);

        // Plus de trois lignes : la dernière se termine par "..".
        CHECK_TRUE(EventLogScreen::wrap(
                       "un deux trois quatre cinq six sept huit neuf dix "
                       "onze douze treize quatorze", lines) == 3);
        const size_t last = std::strlen(lines[2]);
        CHECK_TRUE(last >= 2 && lines[2][last - 1] == '.' && lines[2][last - 2] == '.');
        CHECK_TRUE(last <= EventLogScreen::LINE_CHARS);

        // Date courte, sans les secondes.
        EventLog log;
        CHECK_TRUE(EventLogScreen::lastFirstIndex(log) == 0);
        log.add(42000, ClockSample{}, EventKind::Info, false, "sans heure");
        log.add(50000, clockAt(14, 32, 5), EventKind::Info, false, "avec heure");

        char text[24];
        EventLogScreen::formatTime(*log.at(0), text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "04/10 14:32") == 0);
        EventLogScreen::formatTime(*log.at(1), text, sizeof(text));
        CHECK_TRUE(std::strcmp(text, "+42 s") == 0);

        // Défilement jusqu'au dernier événement en haut de l'écran.
        CHECK_TRUE(EventLogScreen::lastFirstIndex(log) == 1);

        EventEntry fault;
        fault.kind = EventKind::Fault;
        CHECK_TRUE(EventLogScreen::color(fault) == 0xF800);
    }
}

void runEventLogTests()
{
    TestHarness::run("journal : ordre et capacite", testOrderAndCapacity);
    TestHarness::run("journal : regroupement des rafales", testCoalescing);
    TestHarness::run("journal : port serie et sauvegarde", testPrintAndSaveFlags);
    TestHarness::run("journal : formats affiches", testFormats);
    TestHarness::run("journal : CSV et relecture", testCsvRoundTrip);
    TestHarness::run("journal : evenements du processus", testProcessEvents);
    TestHarness::run("journal : visionneuse", testScreenHelpers);
}
