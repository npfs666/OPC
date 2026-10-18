from datetime import datetime
import math
from pathlib import Path
import sys
import unittest

import matplotlib
matplotlib.use("Agg")
from matplotlib.figure import Figure
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from acquisition import Session
from plot_view import PlotView, TimeLocator, auto_limits, decimate, format_clock, format_duration, tick_step


class DecimationTests(unittest.TestCase):
    def test_small_slices_are_copied_with_edge_points(self):
        times = np.arange(10.0)
        x, y = decimate(times, times * 2, 3.5, 6.5, 100)
        self.assertEqual(list(x), [3, 4, 5, 6, 7])
        x[0] = -1
        self.assertEqual(times[3], 3)

    def test_min_max_preserved(self):
        times = np.arange(100000.0)
        values = np.sin(times / 1000)
        values[54321] = 5
        x, y = decimate(times, values, 0, 100000, 1000)
        self.assertLessEqual(len(x), 1000)
        self.assertEqual(np.nanmax(y), 5)
        self.assertAlmostEqual(np.nanmin(y), -1, places=3)
        self.assertTrue(np.all(np.diff(x) >= 0))

    def test_gaps_stay_visible(self):
        times = np.arange(100000.0)
        values = np.ones(100000)
        values[50000] = np.nan
        x, y = decimate(times, values, 0, 100000, 500)
        self.assertEqual(int(np.isnan(y).sum()), 1)


class TimeAxisTests(unittest.TestCase):
    def test_formats(self):
        self.assertEqual(format_duration(0), "0:00:00")
        self.assertEqual(format_duration(3725.4), "1:02:05")
        self.assertEqual(format_duration(90000), "25:00:00")
        self.assertEqual(format_duration(59.96, 0.1), "0:01:00.0")
        self.assertEqual(format_duration(-5), "-0:00:05")
        start = datetime(2026, 10, 17, 23, 59, 30)
        self.assertEqual(format_clock(start, 10), "23:59:40")
        self.assertEqual(format_clock(start, 30, 3600), "18/10")
        self.assertEqual(format_clock(start, 90, 60), "00:01")

    def test_steps_and_clock_aligned_ticks(self):
        self.assertEqual(tick_step(60), 10)
        self.assertEqual(tick_step(86400), 21600)
        view = PlotView(Figure())
        view.x_mode = "heure"
        view.started_at = datetime(2026, 10, 17, 10, 0, 7)
        ticks = TimeLocator(view).tick_values(0, 60)
        self.assertEqual(ticks[0], 3)  # 10:00:10
        self.assertTrue(all(math.isclose(b - a, 10) for a, b in zip(ticks, ticks[1:])))

    def test_auto_limits(self):
        self.assertEqual(auto_limits([]), (0, 1))
        self.assertEqual(auto_limits([np.array([2.0, 2.0])]), (1.5, 2.5))
        low, high = auto_limits([np.array([0.0, np.nan]), np.array([10.0])])
        self.assertLess(low, 0)
        self.assertGreater(high, 10)


class PlotViewTests(unittest.TestCase):
    def test_one_axes_per_unit_and_rebuild_only_on_change(self):
        session = Session()
        session.start(origin=0)
        for line in ["T1: 20 °C", "T2: 21 °C", "H: 50 %HR"]:
            session.ingest(1, line)
        variables = list(session.variables.values())
        labels = {v.key: v.label for v in variables}
        view = PlotView(Figure())
        self.assertTrue(view.sync(variables, labels))
        self.assertEqual(list(view.axes), ["°C", "%HR"])
        self.assertFalse(view.sync(variables, labels))
        view.update(variables, 0, 10, 100)
        self.assertEqual(list(view.lines[("", "T1")].get_xdata()), [1])
        view.split_units = False
        self.assertTrue(view.sync(variables, labels))
        self.assertEqual(list(view.axes), [PlotView.ALL])
        view.y_limits[PlotView.ALL] = (0, 100)
        view.update(variables, 0, 10, 100)
        self.assertEqual(view.axes[PlotView.ALL].get_ylim(), (0, 100))
        view.markers = {("", "T1"): (20, 19, 21)}
        view.update(variables, 0, 10, 100)
        self.assertEqual([list(m.get_ydata()) for m in view.marker_lines[("", "T1")]],
                         [[20, 20], [19, 19], [21, 21]])
        view.markers = {}
        view.update(variables, 0, 10, 100)
        self.assertEqual(view.marker_lines, {})
        self.assertEqual(len(view.axes[PlotView.ALL].lines), 3)
        self.assertTrue(view.sync([], {}))
        self.assertEqual(len(view.figure.axes), 1)


if __name__ == "__main__":
    unittest.main()
