"""Lecture série et données, indépendantes de l'interface graphique."""

import csv
import math
import operator
import os
import re
import threading
import time
from collections import Counter
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from queue import Queue
from typing import NamedTuple

import numpy as np


OPERATIONS = {
    "Soustraction (A − B)": ("−", operator.sub),
    "Addition (A + B)": ("+", operator.add),
    "Multiplication (A × B)": ("×", operator.mul),
    "Division (A ÷ B)": ("÷", operator.truediv),
}

# Couleurs bien distinctes sur fond clair, attribuées dans l'ordre de détection.
COLORS = ["#2563eb", "#e4572e", "#17a673", "#9333ea", "#d6336c",
          "#b7791f", "#0e9cc4", "#5f6b7a", "#7cb518", "#f08c00"]

DEMO_PORT = "Démo"

# La valeur doit commencer juste après « : » (espaces permis).
NUMBER = re.compile(r"[+-]?(?:\d+(?:[.,]\d*)?|[.,]\d+)(?:[eE][+-]?\d+)?")
# Refuse 1.2.3, 1e+, 12:34:56… : la suite ne doit pas prolonger le nombre.
BAD_SUFFIX = re.compile(r"[.,:\d]|[eE](?:[+\-\d]|$)")
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
# En-tête de bloc OPC : « -------Measurements------- », « ===== Process Control ===== ».
SECTION = re.compile(r"^[-=*#]{3,}\s*([^-=*#].*?)\s*[-=*#]{3,}$")
LINE_END = re.compile(rb"\r\n|\r|\n")
MAX_NAME = 48
MAX_PENDING = 65536


def clock():
    """Horloge monotone (s) qui avance aussi pendant la veille du PC.

    time.monotonic() s'arrête pendant la veille sous Linux : les temps et
    l'horodatage du CSV prendraient alors tout le retard de la veille.
    """
    if hasattr(time, "CLOCK_BOOTTIME"):
        return time.clock_gettime(time.CLOCK_BOOTTIME)
    return time.monotonic()


class Reading(NamedTuple):
    """Une ligne « Nom : valeur unité », ou « Nom : état » si value est None."""
    name: str
    value: float | None
    unit: str = ""
    text: str = ""
    section: str = ""


def parse_line(line):
    """Renvoie une Reading, ou None si la ligne n'a pas la forme « Nom : … »."""
    name, separator, rest = ANSI.sub("", line).partition(":")
    name, rest = name.strip(), rest.strip()
    if not separator or not name or not rest:
        return None
    match = NUMBER.match(rest)
    suffix = rest[match.end():] if match else ""
    if not match or BAD_SUFFIX.match(suffix):
        return Reading(name, None, text=rest)
    value = float(match.group().replace(",", "."))
    if not math.isfinite(value):
        return Reading(name, None, text=rest)
    # L'unité est le premier mot qui suit ; « 0.5 | Measur : … » n'en a pas.
    unit = suffix.split(maxsplit=1)[0].rstrip(",;") if suffix.strip() else ""
    if unit.startswith("|"):
        unit = ""
    return Reading(name, value, unit)


def parse_section(line):
    match = SECTION.match(ANSI.sub("", line).strip())
    return match.group(1) if match else None


class LineParser:
    """Analyse les lignes d'un port en suivant la section courante."""

    def __init__(self):
        self.section = ""

    def parse(self, line):
        reading = parse_line(line)
        if reading is None:
            section = parse_section(line)
            if section is not None:
                self.section = section
            return None
        return reading._replace(section=self.section) if self.section else reading


class LineBuffer:
    """Reconstruit les lignes même si une lecture coupe un nombre ou un UTF-8."""

    OVERFLOW = "[64 Ko reçus sans fin de ligne ont été ignorés : vérifiez le débit]"

    def __init__(self):
        self.pending = b""
        self.after_cr = False

    def feed(self, data):
        if not data:
            return []
        if self.after_cr and data[:1] == b"\n":
            data = data[1:]
        data = self.pending + data
        self.after_cr = data.endswith(b"\r")
        parts = LINE_END.split(data)
        self.pending = parts.pop()
        lines = [part.decode("utf-8", errors="replace") for part in parts]
        if len(self.pending) > MAX_PENDING:
            # Mauvais débit ou flux binaire : on continue sans saturer la mémoire.
            self.pending = b""
            lines.append(self.OVERFLOW)
        return lines


class Series:
    """Temps et valeurs en tableaux numpy, agrandis par doublement."""

    __slots__ = ("_times", "_values", "count")

    def __init__(self, capacity=1024):
        self._times = np.empty(capacity)
        self._values = np.empty(capacity)
        self.count = 0

    def __len__(self):
        return self.count

    def append(self, elapsed, value):
        if self.count == len(self._times):
            size = 2 * len(self._times)
            self._times = np.resize(self._times, size)
            self._values = np.resize(self._values, size)
        self._times[self.count] = elapsed
        self._values[self.count] = value
        self.count += 1

    def replace_last(self, value):
        self._values[self.count - 1] = value

    def clear(self):
        self.count = 0

    @property
    def times(self):
        return self._times[:self.count]

    @property
    def values(self):
        return self._values[:self.count]

    def last(self):
        """(temps, valeur) du dernier point, ou None."""
        if not self.count:
            return None
        return float(self._times[self.count - 1]), float(self._values[self.count - 1])

    def value_at(self, elapsed):
        """Dernière valeur connue à cet instant (maintien), ou None."""
        index = int(np.searchsorted(self.times, elapsed, side="right")) - 1
        return float(self._values[index]) if index >= 0 else None


@dataclass(frozen=True)
class Calculation:
    left: tuple
    right: tuple
    operation: str


@dataclass(eq=False)
class Variable:
    name: str
    unit: str
    color: str
    source: str = ""
    section: str = ""
    plotted: bool = True
    calculation: Calculation | None = None
    status: str = ""
    series: Series = field(default_factory=Series, repr=False)

    @property
    def key(self):
        return self.source, self.name

    @property
    def label(self):
        return f"{self.name} ({self.unit})" if self.unit else self.name

    @property
    def times(self):
        return self.series.times

    @property
    def values(self):
        return self.series.values


def valid_detected_name(name):
    # Écarte les horodatages et les phrases (« 2026-10-17 12 », messages longs).
    return len(name) <= MAX_NAME and (name[0].isalpha() or name[0] == "_")


class Session:
    def __init__(self):
        self.variables = {}
        self.calculations = []
        self.started_at = None
        self.origin = None
        self.sample_count = 0
        self.exported_count = 0
        self.auto_detect = True
        # Appelée pour une variable détectée : renvoie (couleur, tracée).
        self.defaults = None

    def start(self, origin=None):
        self.origin = clock() if origin is None else origin
        self.started_at = datetime.now().astimezone()
        self.sample_count = 0
        self.exported_count = 0
        for variable in self.variables.values():
            variable.series.clear()
            variable.status = ""

    @property
    def unsaved(self):
        return self.sample_count > self.exported_count

    def latest_time(self):
        return max((v.series.last()[0] for v in self.variables.values() if v.series.count), default=0.0)

    def next_color(self):
        used = Counter(v.color for v in self.variables.values())
        return min(COLORS, key=lambda color: (used[color], COLORS.index(color)))

    def add_variable(self, name, unit, color=None, source="", section="", plotted=True):
        name = name.strip()
        if not name or ":" in name or "\n" in name or "\r" in name:
            raise ValueError("Saisissez le nom exact avant « : », sans le séparateur.")
        key = (source, name)
        if key in self.variables:
            raise ValueError("Cette variable existe déjà pour ce port.")
        variable = Variable(name, unit.strip(), color or self.next_color(), source, section, plotted)
        self.variables[key] = variable
        return variable

    def remove_variable(self, key):
        """Supprime la variable et les calculs qui en dépendent ; renvoie les clés retirées."""
        removed = [key]
        for result in self.calculations:
            calculation = result.calculation
            if calculation.left in removed or calculation.right in removed:
                removed.append(result.key)
        for removed_key in removed:
            variable = self.variables.pop(removed_key, None)
            if variable is not None and variable.calculation is not None:
                self.calculations.remove(variable)
        return removed

    def dependents(self, key):
        names = []
        for result in self.calculations:
            if result.calculation.left in (key, *names) or result.calculation.right in (key, *names):
                names.append(result.key)
        return names

    def add_calculation(self, left, right, operation, color=None):
        if left not in self.variables or right not in self.variables or left == right:
            raise ValueError("Sélectionnez deux variables différentes.")
        if operation not in OPERATIONS:
            raise ValueError("Choisissez une opération dans la liste.")
        a, b = self.variables[left], self.variables[right]
        unit = calculation_unit(a.unit, b.unit, operation)
        number = 1 + sum(v.calculation is not None for v in self.variables.values())
        symbol = OPERATIONS[operation][0]
        name = f"Math {number} — {a.name} {symbol} {b.name}"
        variable = self.add_variable(name, unit, color, source="Math")
        variable.calculation = Calculation(left, right, operation)
        self.calculations.append(variable)
        self._replay(variable, a, b)
        return variable

    def _replay(self, result, a, b):
        """Rejoue l'historique des deux entrées avec maintien de la dernière valeur."""
        if not a.series.count or not b.series.count:
            return
        times = np.unique(np.concatenate((a.times, b.times)))
        # À temps identique, les deux entrées sont mises à jour avant le calcul.
        left = np.searchsorted(a.times, times, side="right") - 1
        right = np.searchsorted(b.times, times, side="right") - 1
        known = (left >= 0) & (right >= 0)
        times, left, right = times[known], left[known], right[known]
        values = calculate_array(result.calculation.operation, a.values[left], b.values[right])
        for elapsed, value in zip(times.tolist(), values.tolist()):
            result.series.append(elapsed, value)
        self.sample_count += len(times)

    @staticmethod
    def _calculate(operation, a, b):
        if not math.isfinite(a) or not math.isfinite(b):
            return math.nan
        try:
            value = OPERATIONS[operation][1](a, b)
        except (ZeroDivisionError, OverflowError):
            return math.nan
        return value if math.isfinite(value) else math.nan

    def _record(self, variable, elapsed, value):
        last = variable.series.last()
        if variable.calculation and last is not None and last[0] == elapsed:
            # Les deux opérandes peuvent arriver séparément au même instant.
            variable.series.replace_last(value)
        else:
            variable.series.append(elapsed, value)
            self.sample_count += 1

    def ingest(self, received_at, line, source=""):
        """Analyse une ligne ; renvoie la variable mesurée, ou None."""
        reading = parse_line(line)
        if reading is None:
            return None
        changed = self.ingest_reading(received_at, reading, source)
        return self.variables.get((source, reading.name)) if changed else None

    def ingest_reading(self, received_at, reading, source=""):
        """Enregistre une lecture ; renvoie les clés modifiées (calculs inclus)."""
        if self.origin is None:
            return ()
        key = (source, reading.name)
        variable = self.variables.get(key)
        if variable is None:
            if reading.value is None or not self.auto_detect or not valid_detected_name(reading.name):
                return ()
            color, plotted = self.defaults(reading) if self.defaults else (None, True)
            variable = self.add_variable(reading.name, reading.unit, color, source,
                                         reading.section, plotted)
        if variable.calculation is not None:
            return ()
        elapsed = max(0.0, received_at - self.origin)
        if reading.value is None:
            # Mesure en défaut : un seul point NaN suffit à couper la courbe.
            if variable.status == reading.text:
                return ()
            variable.status = reading.text
            last = variable.series.last()
            if last is None or math.isnan(last[1]):
                return {key}
            value = math.nan
        else:
            variable.status = ""
            value = reading.value
        self._record(variable, elapsed, value)
        changed = {key}
        # Les calculs ne référencent que des variables déjà créées : cet ordre
        # permet aussi de calculer une courbe à partir d'une autre courbe Math.
        for result in self.calculations:
            calculation = result.calculation
            if calculation.left not in changed and calculation.right not in changed:
                continue
            a, b = self.variables[calculation.left].series.last(), self.variables[calculation.right].series.last()
            if a is None or b is None:
                continue
            self._record(result, max(a[0], b[0]), self._calculate(calculation.operation, a[1], b[1]))
            changed.add(result.key)
        return changed

    def export_snapshot(self):
        """Copie figée des données, utilisable par un autre thread."""
        variables = list(self.variables.values())
        counts = Counter(variable.label for variable in variables)
        headers = ["horodatage", "temps_s"]
        for variable in variables:
            label = variable.label
            if counts[label] > 1:
                label += f" [{variable.source}]"
            # Évite aussi une collision avec un nom qui contient déjà un suffixe.
            candidate, number = label, 2
            while candidate in headers:
                candidate = f"{label} #{number}"
                number += 1
            headers.append(candidate)
        return ExportSnapshot(headers, self.started_at,
                              [variable.times.copy() for variable in variables],
                              [variable.values.copy() for variable in variables],
                              self.sample_count)

    def export_csv(self, path):
        snapshot = self.export_snapshot()
        snapshot.write(path)
        self.exported_count = max(self.exported_count, snapshot.sample_count)


@dataclass
class ExportSnapshot:
    headers: list
    started_at: datetime | None
    times: list
    values: list
    sample_count: int

    def write(self, path):
        """Une ligne par temps_s exporté, sans interpolation entre variables."""
        columns = len(self.times)
        if columns:
            times = np.concatenate(self.times)
            values = np.concatenate(self.values)
            indexes = np.concatenate([np.full(len(t), i + 2) for i, t in enumerate(self.times)])
            # Tri stable : à temps égal, la dernière mesure reçue reste la dernière.
            order = np.argsort(times, kind="stable")
            samples = zip(times[order].tolist(), indexes[order].tolist(), values[order].tolist())
        else:
            samples = iter(())
        started_at = self.started_at or datetime.now().astimezone()
        with open(path, "w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.writer(stream, delimiter=";")
            writer.writerow(self.headers)
            row = None
            previous = None
            for elapsed, column, value in samples:
                if elapsed != previous:
                    previous = elapsed
                    time_s = f"{elapsed:.6f}"
                    # Les ports distincts peuvent arriver désordonnés : le CSV reste chronologique.
                    if row is None or row[1] != time_s:
                        if row is not None:
                            writer.writerow(row)
                        timestamp = started_at + timedelta(seconds=elapsed)
                        row = [timestamp.isoformat(timespec="milliseconds"), time_s, *([""] * columns)]
                # Même variable au même temps exporté : garde la dernière mesure.
                row[column] = repr(value) if math.isfinite(value) else ""
            if row is not None:
                writer.writerow(row)


class Statistics(NamedTuple):
    count: int
    mean: float
    median: float
    minimum: float
    maximum: float
    std: float
    minimum_time: float
    maximum_time: float
    start: float
    end: float

    @property
    def peak_to_peak(self):
        return self.maximum - self.minimum


def statistics(times, values, start=None, end=None):
    """Mesures « oscilloscope » sur les points de [start, end] ; les trous sont ignorés."""
    first = 0 if start is None else int(np.searchsorted(times, start, side="left"))
    last = len(times) if end is None else int(np.searchsorted(times, end, side="right"))
    t, v = times[first:last], values[first:last]
    finite = np.isfinite(v)
    if not finite.all():
        t, v = t[finite], v[finite]
    if not v.size:
        return None
    low, high = int(np.argmin(v)), int(np.argmax(v))
    return Statistics(int(v.size), float(v.mean()), float(np.median(v)), float(v[low]), float(v[high]),
                      float(v.std()), float(t[low]), float(t[high]), float(t[0]), float(t[-1]))


def calculation_unit(left, right, operation):
    symbol = OPERATIONS[operation][0]
    if symbol in ("−", "+"):
        if left != right:
            raise ValueError("L’addition et la soustraction nécessitent la même unité affichée. "
                             "Aucune conversion automatique n’est effectuée.")
        return left
    if symbol == "×":
        return "·".join(f"({u})" for u in (left, right) if u)
    if left == right:
        return ""
    return f"({left or '1'})/({right})" if right else left


def calculate_array(operation, a, b):
    with np.errstate(all="ignore"):
        values = OPERATIONS[operation][1](a, b)
    values = np.asarray(values, dtype=float)
    values[~np.isfinite(values)] = np.nan
    return values


class SerialReader(threading.Thread):
    """Seul ce thread manipule le port ; seul Tk manipule les widgets.

    Événements (port, type, données) : « state » (état, message), « lines »
    [(temps, ligne, lecture ou None)], puis « done » à l'arrêt.
    Après une déconnexion, le port est rouvert automatiquement.
    """

    def __init__(self, port, baudrate, events: Queue, demo=False, retry_delay=1.0):
        super().__init__(daemon=True)
        self.port = port
        self.baudrate = baudrate
        self.events = events
        self.demo = demo
        self.retry_delay = retry_delay
        self.serial_number = None
        self.stop_requested = threading.Event()

    def stop(self):
        self.stop_requested.set()

    def emit(self, kind, payload=None):
        self.events.put((self.port, kind, payload))

    def run(self):
        try:
            if self.demo:
                self.emit("state", ("connected", ""))
                self._demo()
                return
            import serial
            self.serial_number = self._find_serial_number(self.port)
            device = self.port
            attempt = 0
            while not self.stop_requested.is_set():
                self.emit("state", ("connecting" if attempt == 0 else "retrying", ""))
                try:
                    self._read(serial, device)
                except (serial.SerialException, OSError, ValueError) as error:
                    if self.stop_requested.is_set():
                        break
                    self.emit("state", ("retrying", str(error)))
                attempt += 1
                self.stop_requested.wait(self.retry_delay)
                # Le même appareil peut revenir sous un autre nom (ttyACM1…).
                device = self._find_device(self.serial_number) or self.port
        except Exception as error:  # Erreur inattendue : affichée sans figer l'interface.
            self.emit("state", ("error", str(error)))
        finally:
            self.emit("done")

    def _read(self, serial, device):
        options = {"exclusive": True} if os.name == "posix" else {}
        with serial.Serial(device, self.baudrate, timeout=0.1, **options) as port:
            self.emit("state", ("connected", device if device != self.port else ""))
            buffer = LineBuffer()
            parser = LineParser()
            while not self.stop_requested.is_set():
                data = port.read(port.in_waiting or 1)
                if not data:
                    continue
                received_at = clock()
                lines = buffer.feed(data)
                if lines:
                    self.emit("lines", [(received_at, line, parser.parse(line)) for line in lines])
            # Un fragment sans fin de ligne n'est pas une mesure complète.

    @staticmethod
    def _find_serial_number(device):
        try:
            from serial.tools import list_ports
            real = os.path.realpath(device) if os.name == "posix" else device
            for info in list_ports.comports():
                if info.device in (device, real) and info.serial_number:
                    return info.serial_number
        except Exception:
            pass
        return None

    @staticmethod
    def _find_device(serial_number):
        if not serial_number:
            return None
        try:
            from serial.tools import list_ports
            for info in list_ports.comports():
                if info.serial_number == serial_number:
                    return info.device
        except Exception:
            pass
        return None

    def _demo(self):
        """Imite la sortie série d'un OPC : blocs, colonnes, défauts et messages."""
        origin = clock()
        parser = LineParser()
        cycle = 0
        while not self.stop_requested.is_set():
            now = clock()
            t = now - origin
            fault = 40 < t % 60 < 44
            temperature = 21 + 1.5 * math.sin(t / 25) + 0.05 * math.sin(t * 3)
            command = min(100, max(0, 50 + 40 * math.cos(t / 25)))
            lines = ["===== Process Control =====", "-------Measurements-------",
                     f"{'TempRTD1':<16}: {temperature:>10.2f} °C",
                     f"{'TempRTD2':<16}: Circuit ouvert" if fault else
                     f"{'TempRTD2':<16}: {temperature + 2 + 0.4 * math.sin(t / 7):>10.2f} °C",
                     f"{'Humidité':<16}: {55 + 8 * math.sin(t / 40):>10.1f} %HR",
                     "--------Regulators--------",
                     f"{'PID chauffage':<16}: {command / 100:.3f} | Measur : {temperature:.2f}°C | SP : 21.00",
                     "----------Outputs---------",
                     f"{'Relais 1':<16}: {1 if command > 50 else 0:>10d}"]
            if cycle % 50 == 0:
                lines.insert(0, f"{datetime.now():%Y-%m-%d %H:%M:%S} Démonstration : cycle {cycle}")
            self.emit("lines", [(now, line, parser.parse(line)) for line in lines])
            cycle += 1
            self.stop_requested.wait(0.5)
