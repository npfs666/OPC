"""Réglages conservés entre deux lancements (port, fenêtre, couleurs…)."""

import json
import os
import sys
from pathlib import Path


def default_path():
    if os.name == "nt":
        base = Path(os.environ.get("APPDATA") or Path.home())
    elif sys.platform == "darwin":
        base = Path.home() / "Library" / "Application Support"
    else:
        base = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config")
    return base / "trace-serie" / "preferences.json"


class Preferences(dict):
    def __init__(self, path=None):
        super().__init__()
        self.path = Path(path) if path else default_path()
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
            if isinstance(data, dict):
                self.update(data)
        except (OSError, ValueError):
            pass  # Premier lancement ou fichier illisible : réglages par défaut.

    def save(self):
        """Écriture atomique ; une erreur n'empêche pas de fermer l'application."""
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            temporary = self.path.with_suffix(".tmp")
            temporary.write_text(json.dumps(self, ensure_ascii=False, indent=1), encoding="utf-8")
            os.replace(temporary, self.path)
            return True
        except OSError:
            return False
