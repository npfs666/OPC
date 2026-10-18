import csv
from datetime import timedelta
import math
import os
from pathlib import Path
from queue import Queue
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import numpy as np

from acquisition import (LineBuffer, LineParser, Reading, SerialReader, Session, parse_line, parse_section,
                         statistics)


def read_csv(session):
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "data.csv"
        session.export_csv(path)
        with path.open(encoding="utf-8-sig", newline="") as stream:
            return list(csv.reader(stream, delimiter=";"))


class ParsingTests(unittest.TestCase):
    def test_numeric_formats(self):
        examples = [
            ("TempRTD1        :     22.115 °C", ("TempRTD1", 22.115, "°C")),
            (" TempRTD1 : -2,51 °C", ("TempRTD1", -2.51, "°C")),
            ("Volt: +1.25e-3 V", ("Volt", 0.00125, "V")),
            ("x: .5A", ("x", 0.5, "A")),
            ("\x1b[32mx: 3\x1b[0m", ("x", 3, "")),
            ("Humidité        :       55.2 %HR", ("Humidité", 55.2, "%HR")),
            ("PID1            : 0.42 | Measur : 21.3°C | SP : 21", ("PID1", 0.42, "")),
            ("Calibration N0 input 1: 12.5 pts, 2-wire 3 pts", ("Calibration N0 input 1", 12.5, "pts")),
        ]
        for line, expected in examples:
            with self.subTest(line=line):
                reading = parse_line(line)
                self.assertEqual((reading.name, reading.value, reading.unit), expected)

    def test_states_are_not_values(self):
        for line, text in [("x: erreur 45", "erreur 45"), ("x: nan", "nan"), ("x: inf", "inf"),
                           ("x: 1e999", "1e999"), ("x: 1.2.3", "1.2.3"), ("x: 1e+", "1e+"),
                           ("TempRTD2        : Circuit ouvert", "Circuit ouvert"),
                           ("2026-10-17 12:03:44 Alarme", "03:44 Alarme")]:
            with self.subTest(line=line):
                reading = parse_line(line)
                self.assertIsNone(reading.value)
                self.assertEqual(reading.text, text)

    def test_lines_without_measurement(self):
        for line in ["boot 123", "x:", ": 12", "", "===== Process Control ====="]:
            with self.subTest(line=line):
                self.assertIsNone(parse_line(line))

    def test_sections(self):
        self.assertEqual(parse_section("-------Measurements-------"), "Measurements")
        self.assertEqual(parse_section("===== Process Control ====="), "Process Control")
        self.assertIsNone(parse_section("--------------"))
        parser = LineParser()
        self.assertEqual(parser.parse("T : 1").section, "")
        self.assertIsNone(parser.parse("--------Regulators--------"))
        self.assertEqual(parser.parse("PID : 0.5").section, "Regulators")

    def test_fragmented_utf8_and_line_endings(self):
        buffer = LineBuffer()
        self.assertEqual(buffer.feed(b"TempRTD1 : 22."), [])
        self.assertEqual(buffer.feed(b"115 \xc2"), [])
        self.assertEqual(buffer.feed(b"\xb0C\r"), ["TempRTD1 : 22.115 °C"])
        self.assertEqual(buffer.feed(b"\nx:1\ny:2\rz:3\r\n"), ["x:1", "y:2", "z:3"])
        self.assertEqual(buffer.feed(b"a\r"), ["a"])
        self.assertEqual(buffer.feed(b"\r\n"), [""])

    def test_unterminated_line_is_dropped_without_stopping(self):
        buffer = LineBuffer()
        self.assertEqual(buffer.feed(b"a" * 65537), [LineBuffer.OVERFLOW])
        self.assertEqual(buffer.feed(b"x: 1\n"), ["x: 1"])


class SessionTests(unittest.TestCase):
    def setUp(self):
        self.session = Session()
        self.session.add_variable("TempRTD1", "°C", "#2563eb")
        self.session.start(origin=100)

    def test_auto_detection_exact_names(self):
        self.session.ingest(101, "TempRTD1: 22.115 °C")
        self.session.ingest(101, "TempRTD10: 45 °C")
        self.session.ingest(101, "temprtd1: 46")
        self.assertEqual(list(self.session.variables[("", "TempRTD1")].values), [22.115])
        self.assertEqual(self.session.variables[("", "TempRTD10")].unit, "°C")
        self.assertIn(("", "temprtd1"), self.session.variables)
        self.assertEqual(self.session.sample_count, 3)

    def test_junk_names_and_states_do_not_create_signals(self):
        for line in ["2026-10-17 12: 5", "Sonde: Circuit ouvert", "x" * 60 + ": 3"]:
            self.session.ingest(101, line)
        self.assertEqual(list(self.session.variables), [("", "TempRTD1")])
        self.session.auto_detect = False
        self.session.ingest(101, "Autre: 3")
        self.assertNotIn(("", "Autre"), self.session.variables)

    def test_defaults_callback(self):
        self.session.defaults = lambda reading: ("#123456", reading.section == "Measurements")
        self.session.ingest_reading(101, Reading("PID", 0.5, section="Regulators"), "COM1")
        variable = self.session.variables[("COM1", "PID")]
        self.assertEqual((variable.color, variable.plotted, variable.section), ("#123456", False, "Regulators"))

    def test_fault_state_makes_one_gap(self):
        variable = self.session.variables[("", "TempRTD1")]
        self.session.ingest(101, "TempRTD1: 22")
        self.session.ingest(102, "TempRTD1: Circuit ouvert")
        self.session.ingest(103, "TempRTD1: Circuit ouvert")
        self.assertEqual(variable.status, "Circuit ouvert")
        self.session.ingest(104, "TempRTD1: 23")
        self.assertEqual(variable.status, "")
        self.assertEqual(list(variable.times), [1, 2, 4])
        self.assertTrue(math.isnan(variable.values[1]))
        self.assertEqual(read_csv(self.session)[2][1:], ["2.000000", ""])

    def test_clock_counts_suspend(self):
        # Une veille du PC (horloge figée par time.monotonic sous Linux) ne doit
        # pas décaler le temps des mesures suivantes.
        import acquisition
        session = Session()
        session.add_variable("T", "°C")
        with patch.object(acquisition, "clock", return_value=1000.0):
            session.start()
        session.ingest(1000.0 + 6 * 3600, "T: 20")
        self.assertEqual(list(session.variables[("", "T")].times), [6 * 3600])
        if hasattr(time, "CLOCK_BOOTTIME"):
            self.assertAlmostEqual(acquisition.clock(), time.clock_gettime(time.CLOCK_BOOTTIME), delta=1)

    def test_nothing_recorded_before_start(self):
        session = Session()
        self.assertIsNone(session.ingest(1, "T: 3"))
        self.assertEqual(session.variables, {})

    def test_duplicate_name(self):
        with self.assertRaises(ValueError):
            self.session.add_variable(" TempRTD1 ", "K", "red")

    def test_same_name_on_two_sources(self):
        first = self.session.add_variable("Temp", "°C", "red", "COM1")
        second = self.session.add_variable("Temp", "°C", "blue", "COM2")
        self.session.ingest(101, "Temp: 12", "COM1")
        self.session.ingest(102, "Temp: 24", "COM2")
        self.assertEqual(list(first.values), [12])
        self.assertEqual(list(second.values), [24])
        with self.assertRaises(ValueError):
            self.session.add_variable("Temp", "K", "green", "COM1")

    def test_colors_are_spread(self):
        colors = {self.session.add_variable(f"V{i}", "").color for i in range(5)}
        self.assertEqual(len(colors), 5)

    def test_series_growth_and_value_at(self):
        variable = self.session.variables[("", "TempRTD1")]
        for i in range(3000):
            self.session.ingest(100 + i, f"TempRTD1: {i}")
        self.assertEqual(len(variable.series), 3000)
        self.assertEqual(variable.series.value_at(10.5), 10)
        self.assertIsNone(variable.series.value_at(-1))
        self.assertEqual(self.session.latest_time(), 2999)

    def test_wide_csv_empty_cells_and_chronological_order(self):
        self.session.add_variable("TempRTD1", "°C", "blue", "COM2")
        self.session.add_variable("Pression", "hPa", "green", "COM2")
        self.session.ingest(102, "TempRTD1: 24", "COM2")
        self.session.ingest(101.25, "TempRTD1: 22.115")
        self.session.ingest(102, "Pression: 1000", "COM2")
        self.session.add_variable("SansMesure", "V", "black", "COM2")
        rows = read_csv(self.session)
        self.assertEqual(rows[0], ["horodatage", "temps_s", "TempRTD1 (°C) []",
                                   "TempRTD1 (°C) [COM2]", "Pression (hPa)", "SansMesure (V)"])
        self.assertEqual(rows[1][1:], ["1.250000", "22.115", "", "", ""])
        self.assertEqual(rows[2][1:], ["2.000000", "", "24.0", "1000.0", ""])
        self.assertEqual(len(rows), 3)
        timestamp = self.session.started_at + timedelta(seconds=1.25)
        self.assertEqual(rows[1][0], timestamp.isoformat(timespec="milliseconds"))

    def test_csv_groups_sources_at_exported_time_precision(self):
        self.session.add_variable("TempRTD1", "°C", "blue", "COM2")
        self.session.ingest(101.0000002, "TempRTD1: 24", "COM2")
        self.session.ingest(101.0000001, "TempRTD1: 22")
        self.session.ingest(101.0000008, "TempRTD1: 25", "COM2")
        rows = read_csv(self.session)
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[1][1:], ["1.000000", "22.0", "24.0"])
        self.assertEqual(rows[2][1:], ["1.000001", "", "25.0"])
        self.assertFalse(self.session.unsaved)

    def test_csv_same_variable_same_time_keeps_last_value(self):
        self.session.ingest(101, "TempRTD1: 22")
        self.session.ingest(102, "TempRTD1: 30")
        self.session.ingest(101, "TempRTD1: 23")
        rows = read_csv(self.session)
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[1][1:], ["1.000000", "23.0"])
        self.assertEqual(rows[2][1:], ["2.000000", "30.0"])

    def test_csv_header_collisions_remain_distinct(self):
        self.session.add_variable("horodatage", "", "red", "COM1")
        self.session.add_variable("T", "", "red", "COM1")
        self.session.add_variable("T", "", "blue", "COM2")
        self.session.add_variable("T [COM1]", "", "green", "COM3")
        headers = read_csv(self.session)[0]
        self.assertEqual(len(headers), len(set(headers)))

    def test_export_and_unsaved_state(self):
        self.session.ingest(101.25, "TempRTD1: -22,115 °C")
        self.assertTrue(self.session.unsaved)
        rows = read_csv(self.session)
        self.assertEqual(rows[1][1:], ["1.250000", "-22.115"])
        self.assertFalse(self.session.unsaved)
        self.session.ingest(102, "TempRTD1: 23")
        self.assertTrue(self.session.unsaved)
        snapshot = self.session.export_snapshot()
        self.session.ingest(103, "TempRTD1: 24")
        self.assertEqual(len(snapshot.times[0]), 2)
        self.session.start(origin=200)
        self.assertEqual(list(self.session.variables[("", "TempRTD1")].values), [])
        self.assertFalse(self.session.unsaved)

    def test_remove_variable_removes_dependent_calculations(self):
        other = self.session.add_variable("T2", "°C")
        first = self.session.add_calculation(("", "TempRTD1"), other.key, "Addition (A + B)")
        second = self.session.add_calculation(first.key, other.key, "Addition (A + B)")
        self.assertEqual(self.session.dependents(other.key), [first.key, second.key])
        removed = self.session.remove_variable(("", "TempRTD1"))
        self.assertEqual(removed, [("", "TempRTD1"), first.key, second.key])
        self.assertEqual(list(self.session.variables), [other.key])
        self.assertEqual(self.session.calculations, [])


class StatisticsTests(unittest.TestCase):
    def test_measurements_on_window_ignore_gaps(self):
        times = np.arange(10.0)
        values = np.array([5, 1, 9, np.nan, 3, 7, 2, 8, 4, 6], dtype=float)
        whole = statistics(times, values)
        self.assertEqual(whole.count, 9)
        self.assertEqual((whole.minimum, whole.minimum_time), (1, 1))
        self.assertEqual((whole.maximum, whole.maximum_time), (9, 2))
        self.assertEqual(whole.peak_to_peak, 8)
        self.assertAlmostEqual(whole.mean, 45 / 9)
        self.assertEqual(whole.median, 5)
        window = statistics(times, values, 4, 6)
        self.assertEqual((window.count, window.mean, window.start, window.end), (3, 4, 4, 6))
        self.assertAlmostEqual(window.std, np.std([3, 7, 2]))

    def test_empty_window(self):
        self.assertIsNone(statistics(np.arange(3.0), np.full(3, np.nan)))
        self.assertIsNone(statistics(np.arange(3.0), np.arange(3.0), 10, 20))


class ReaderTests(unittest.TestCase):
    def collect(self, events, kinds, timeout=3):
        """Lit les événements jusqu'à trouver chacun des types demandés, dans l'ordre."""
        seen = []
        deadline = time.monotonic() + timeout
        for kind in kinds:
            while True:
                item = events.get(timeout=max(0.01, deadline - time.monotonic()))
                seen.append(item)
                if item[1] == kind:
                    break
        return seen

    def test_demo_and_stop(self):
        events = Queue()
        reader = SerialReader("Démo", 115200, events, demo=True)
        reader.start()
        seen = self.collect(events, ["state", "lines"])
        self.assertEqual(seen[0][2], ("connected", ""))
        names = {reading.name for _, _, reading in seen[-1][2] if reading}
        self.assertIn("TempRTD1", names)
        reader.stop()
        reader.join(timeout=2)
        self.assertFalse(reader.is_alive())
        kinds = []
        while not events.empty():
            kinds.append(events.get()[1])
        self.assertEqual(kinds[-1], "done")

    @unittest.skipUnless(os.name == "posix", "Pseudo-terminal POSIX nécessaire")
    def test_real_serial_api_with_virtual_port(self):
        import pty
        master, slave = pty.openpty()
        events = Queue()
        path = os.ttyname(slave)
        reader = SerialReader(path, 115200, events, retry_delay=0.1)
        try:
            reader.start()
            self.collect(events, ["state"])
            self.assertEqual(self.collect(events, ["state"])[-1][2], ("connected", ""))
            os.write(master, b"-------Measurements-------\nTempRTD1 : 22.")
            # Traverse le délai de lecture pour vérifier la conservation du fragment.
            time.sleep(0.15)
            os.write(master, "115 °C\r\nTempRTD10: 90\n".encode())
            lines = []
            while len(lines) < 3:
                source, kind, payload = events.get(timeout=2)
                self.assertEqual((source, kind), (path, "lines"))
                lines += payload
            self.assertEqual([line for _, line, _ in lines],
                             ["-------Measurements-------", "TempRTD1 : 22.115 °C", "TempRTD10: 90"])
            self.assertEqual(lines[1][2], Reading("TempRTD1", 22.115, "°C", "", "Measurements"))
            os.close(master)
            master = None
            # Déconnexion : le lecteur signale l'erreur et réessaie au lieu de s'arrêter.
            state = self.collect(events, ["state"])[-1][2]
            self.assertEqual(state[0], "retrying")
            self.assertTrue(reader.is_alive())
        finally:
            reader.stop()
            reader.join(timeout=2)
            if master is not None:
                os.close(master)
            os.close(slave)
        self.assertFalse(reader.is_alive())
        kinds = []
        while not events.empty():
            kinds.append(events.get()[1])
        self.assertEqual(kinds[-1], "done")


if __name__ == "__main__":
    unittest.main()
