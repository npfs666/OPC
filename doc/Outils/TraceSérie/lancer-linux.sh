#!/bin/sh
# Utilise le dossier de ce fichier, quel que soit le dossier courant.
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "$APP_DIR" || exit 1
exec python3 "$APP_DIR/lancer.pyw"
