import csv
import math
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from acquisition import OPERATIONS, Session


class MathTests(unittest.TestCase):
    def setUp(self):
        self.session = Session()
        self.a = self.session.add_variable("Temp", "°C", "red", "COM1")
        self.b = self.session.add_variable("Temp", "°C", "blue", "COM2")
        self.session.start(origin=100)

    def add_math(self, index=0, left=None, right=None):
        return self.session.add_calculation(left or self.a.key, right or self.b.key,
                                             list(OPERATIONS)[index], "green")

    def feed(self, time, a=None, b=None):
        if a is not None:
            self.session.ingest(100 + time, f"Temp: {a}", "COM1")
        if b is not None:
            self.session.ingest(100 + time, f"Temp: {b}", "COM2")

    def test_four_operations_and_operand_order(self):
        results = [self.add_math(index) for index in range(4)]
        self.feed(1, a=10, b=2)
        self.assertEqual([v.values[-1] for v in results], [8, 12, 20, 5])
        reverse = self.add_math(left=self.b.key, right=self.a.key)
        self.assertEqual(list(reverse.values), [-8])
        self.assertEqual(results[0].unit, "°C")
        self.assertEqual(results[2].unit, "(°C)·(°C)")
        self.assertEqual(results[3].unit, "")

    def test_asynchronous_inputs_and_same_time_updates(self):
        result = self.add_math()
        self.feed(1, a=10)
        self.assertEqual(list(result.values), [])
        self.feed(2, b=3)
        self.feed(3, a=12)
        self.feed(3, b=5)
        self.assertEqual(list(result.times), [2, 3])
        self.assertEqual(list(result.values), [7, 7])
        # Le point mis à jour au même instant ne compte qu'une fois.
        self.assertEqual(self.session.sample_count, 6)

    def test_calculates_existing_history_without_future_values(self):
        self.feed(1, a=10)
        self.feed(2, b=3)
        self.feed(3, a=12, b=5)
        self.feed(4, b=7)
        result = self.add_math()
        self.assertEqual(list(result.times), [2, 3, 4])
        self.assertEqual(list(result.values), [7, 7, 5])
        self.feed(5, a=14)
        self.assertEqual(list(result.values), [7, 7, 5, 7])

    def test_chaining_and_restart_keep_formulas_only(self):
        difference = self.add_math()
        combined = self.add_math(1, left=difference.key, right=self.b.key)
        self.feed(1, a=10, b=2)
        self.feed(2, b=3)
        self.assertEqual(list(combined.values), [10, 10])
        self.session.start(origin=100)
        self.feed(1, b=4)
        self.assertEqual(list(combined.values), [])
        self.feed(2, a=11)
        self.assertEqual(list(difference.values), [7])
        self.assertEqual(list(combined.values), [11])

    def test_division_by_zero_and_overflow_are_gaps(self):
        quotient = self.add_math(3)
        self.feed(1, a=10, b=2)
        self.feed(2, b=0)
        self.assertTrue(math.isnan(quotient.values[-1]))
        self.feed(3, b=5)
        self.assertEqual(quotient.values[-1], 2)
        self.feed(4, a=1e308, b=1e-308)
        self.assertTrue(math.isnan(quotient.values[-1]))
        replay = self.add_math(3)
        self.assertEqual(len(replay.values), 4)
        self.assertTrue(math.isnan(replay.values[1]) and math.isnan(replay.values[3]))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "math.csv"
            self.session.export_csv(path)
            with path.open(encoding="utf-8-sig") as stream:
                rows = list(csv.DictReader(stream, delimiter=";"))
        self.assertEqual([row[quotient.label] for row in rows], ["5.0", "", "2.0", ""])
        self.assertEqual([row['temps_s'] for row in rows], ['1.000000', '2.000000', '3.000000', '4.000000'])

    def test_invalid_formula_does_not_add_variable(self):
        different = self.session.add_variable("Pression", "Pa", "black", "COM1")
        for left, right, operation in [
            (self.a.key, self.a.key, list(OPERATIONS)[0]),
            (self.a.key, ("unknown", "x"), list(OPERATIONS)[0]),
            (self.a.key, self.b.key, "eval"),
            (self.a.key, different.key, list(OPERATIONS)[0]),
        ]:
            with self.subTest(left=left, right=right, operation=operation):
                with self.assertRaises(ValueError):
                    self.session.add_calculation(left, right, operation, "red")
        self.assertEqual(len(self.session.variables), 3)

    def test_fault_on_input_propagates_as_gap(self):
        result = self.add_math()
        self.feed(1, a=10, b=2)
        self.session.ingest(102, "Temp: Circuit ouvert", "COM1")
        self.assertTrue(math.isnan(result.values[-1]))
        self.feed(3, a=11)
        self.assertEqual(result.values[-1], 9)


if __name__ == "__main__":
    unittest.main()
