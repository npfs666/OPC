"""Fenêtre temporelle visible ; les mesures restent dans la session."""

import math


# (libellé, durée en secondes) ; None affiche tout l'historique.
PRESETS = [("1 min", 60.0), ("10 min", 600.0), ("1 h", 3600.0),
           ("6 h", 21600.0), ("24 h", 86400.0), ("Tout", None)]


class TimeView:
    """Suit les dernières mesures, ou reste figée sur une plage de l'historique."""

    MIN_DURATION = 0.5

    def __init__(self, duration=600.0):
        self.duration = duration
        self.end = 0.0
        self.follow = True

    def configure(self, text):
        duration = None if text.strip().casefold() == "tout" else float(text.replace(",", "."))
        self.set_duration(duration)

    def set_duration(self, duration):
        if duration is not None and (not math.isfinite(duration) or duration <= 0):
            raise ValueError("La durée doit être positive et finie.")
        self.duration = duration
        if duration is None:
            self.follow = True

    def bounds(self, latest):
        if self.duration is None:
            return 0.0, max(1.0, latest * 1.02)
        if self.follow:
            # L'axe grandit jusqu'à la durée choisie, puis défile.
            if latest <= self.duration:
                return 0.0, min(self.duration, max(1.0, latest * 1.02))
            return latest - self.duration, latest
        end = min(max(self.end, self.duration), max(latest, self.duration))
        return end - self.duration, end

    def fractions(self, latest):
        start, end = self.bounds(latest)
        total = max(latest, end)
        return start / total, end / total

    def show(self, start, end, latest):
        """Affiche la plage demandée ; atteindre la fin reprend le suivi."""
        span = max(self.MIN_DURATION, end - start)
        end = max(start, 0.0) + span
        self.duration = span
        if end >= latest:
            self.follow = True
        else:
            self.follow = False
            self.end = end

    def pan(self, delta, latest):
        start, end = self.bounds(latest)
        self.show(start + delta, end + delta, latest)

    def zoom(self, factor, center, latest):
        """Zoom autour de center ; en suivi, le bord droit reste sur la dernière mesure."""
        start, end = self.bounds(latest)
        span = max(self.MIN_DURATION, (end - start) * factor)
        if self.follow:
            self.duration = span
            return
        start = center - (center - start) * factor
        self.show(start, start + span, latest)

    def scroll(self, latest, action, value, unit=None):
        """Commandes d'une barre de défilement Tk."""
        if self.duration is None:
            return
        start, end = self.bounds(latest)
        total = max(latest, end)
        if action == "moveto":
            target = float(value) * total
        elif action == "scroll":
            target = start + int(value) * (end - start) * (0.9 if unit == "pages" else 0.1)
        else:
            return
        self.pan(target - start, latest)

    def freeze(self, latest):
        start, end = self.bounds(latest)
        self.duration = end - start
        self.follow = False
        self.end = end

    def reset(self):
        self.end = 0.0
        self.follow = True
