import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from time_view import TimeView


class TimeViewTests(unittest.TestCase):
    def test_grows_until_limit_then_follows(self):
        view = TimeView(86400)
        self.assertEqual(view.bounds(0), (0, 1))
        self.assertEqual(view.bounds(100), (0, 102))
        self.assertEqual(view.fractions(100), (0, 1))
        self.assertEqual(view.bounds(86400), (0, 86400))
        self.assertEqual(view.bounds(172800), (86400, 172800))
        self.assertEqual(view.fractions(172800), (0.5, 1))

    def test_history_stays_fixed_during_new_measurements(self):
        view = TimeView(86400)
        view.scroll(259200, "moveto", "0.25")
        self.assertFalse(view.follow)
        self.assertEqual(view.bounds(259200), (64800, 151200))
        self.assertEqual(view.bounds(345600), (64800, 151200))
        view.follow = True
        self.assertEqual(view.bounds(345600), (259200, 345600))

    def test_scroll_clamps_and_resumes_at_end(self):
        view = TimeView(86400)
        view.scroll(172800, "moveto", "-1")
        self.assertEqual(view.bounds(172800), (0, 86400))
        view.scroll(172800, "scroll", "1", "units")
        self.assertEqual(view.bounds(172800), (8640, 95040))
        view.scroll(172800, "scroll", "1", "pages")
        self.assertTrue(view.follow)
        self.assertEqual(view.bounds(200000), (113600, 200000))

    def test_configuration_and_reset(self):
        view = TimeView()
        view.configure("3600,5")
        self.assertEqual(view.bounds(10000), (6399.5, 10000))
        for invalid in ("0", "-1", "nan", "inf", "", "abc"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                view.configure(invalid)
            self.assertEqual(view.duration, 3600.5)
        view.scroll(10000, "moveto", "0")
        self.assertFalse(view.follow)
        view.reset()
        self.assertTrue(view.follow)
        self.assertEqual(view.duration, 3600.5)
        view.configure("Tout")
        self.assertEqual(view.bounds(172800), (0, 176256))
        self.assertEqual(view.fractions(172800), (0, 1))

    def test_zoom_keeps_right_edge_while_following(self):
        view = TimeView(600)
        view.zoom(0.5, 1500, 2000)
        self.assertTrue(view.follow)
        self.assertEqual(view.bounds(2000), (1700, 2000))

    def test_zoom_around_cursor_when_frozen(self):
        view = TimeView(600)
        view.show(1000, 1600, 5000)
        self.assertFalse(view.follow)
        view.zoom(0.5, 1100, 5000)
        self.assertEqual(view.bounds(5000), (1050, 1350))
        view.zoom(1.25, 1200, 5000)
        self.assertEqual(view.duration, 375)

    def test_pan_and_freeze(self):
        view = TimeView(600)
        view.freeze(5000)
        self.assertEqual(view.bounds(9000), (4400, 5000))
        view.pan(-6000, 9000)
        self.assertEqual(view.bounds(9000), (0, 600))
        view.pan(100000, 9000)
        self.assertTrue(view.follow)
        self.assertEqual(view.bounds(9000), (8400, 9000))

    def test_all_history_zoom_and_pan(self):
        view = TimeView(None)
        view.zoom(0.5, 0, 1000)
        self.assertEqual(view.duration, 510)
        view = TimeView(None)
        # Tout est déjà visible : rien à gauche de 0, la vue reste en direct.
        view.pan(-100, 1000)
        self.assertTrue(view.follow)
        self.assertEqual(view.duration, 1020)


if __name__ == "__main__":
    unittest.main()
