// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PARAMETER_JSON_H
#define PARAMETER_JSON_H

#include <Hardware/pinout.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <ArduinoJson.h>

/**
 * Conversion entre les réglages et les entrées JSON des fichiers de
 * Storage. Sans accès au système de fichiers : testable sur l'hôte.
 *
 * Une entrée porte la catégorie, le propriétaire, la clé, les libellés, le
 * type, la valeur, et l'unité ou les options pour faciliter la lecture du
 * fichier. Seuls owner_key, key, type et value servent à la relecture.
 */
namespace ParameterJson
{
    // Fichier de destination d'un réglage (voir ParameterOwner::board).
    enum class Scope : uint8_t
    {
        Installation,
        Board
    };

    bool inScope(
        const Parameter& parameter,
        Scope scope);

    /** Écrit les réglages persistants de la portée ; faux si incomplets. */
    bool write(
        JsonArray destination,
        const ParameterList& parameters,
        Scope scope);

    /** Suivi de la relecture, partagé entre les fichiers. */
    struct ReadState
    {
        // Une entrée a déjà été lue pour ce réglage : doublon ignoré.
        bool seen[MAX_PARAMETERS] = {};
        // Entrée d'un réglage connu mais illisible (type changé, valeur
        // absente...) : ce réglage reste à sa valeur par défaut.
        bool rejected[MAX_PARAMETERS] = {};
    };

    /**
     * Lit dans les brouillons les entrées de la portée. Tolérant : une
     * entrée inconnue, transitoire, d'une autre portée ou en double est
     * ignorée ; une entrée illisible est notée dans state.rejected. Les
     * bornes et options sont vérifiées ensuite par
     * ParameterEditor::keepValidDrafts().
     *
     * @return faux si source n'est pas un tableau d'entrées
     */
    bool read(
        JsonVariantConst source,
        const ParameterList& parameters,
        ParameterEditor& editor,
        Scope scope,
        ReadState& state);

    const char* typeName(Parameter::Type type);
}

#endif
