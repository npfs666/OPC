#!/bin/sh
# Contourne entièrement .venv et les bibliothèques du site utilisateur.
# Bibliothèques requises : python3-tk python3-numpy python3-matplotlib python3-serial
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "$APP_DIR" || exit 1
exec env -u PYTHONPATH -u PYTHONHOME /usr/bin/python3 -s "$APP_DIR/lancer.pyw" --system
