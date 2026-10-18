import builtins
from pathlib import Path
import runpy
import shlex
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch


DIRECTORY = Path(__file__).resolve().parents[1]


class LauncherTests(unittest.TestCase):
    def test_system_mode_bypasses_existing_local_environment(self):
        launcher = runpy.run_path(str(DIRECTORY / "lancer.pyw"))
        entered = []
        app = SimpleNamespace(mainloop=lambda: entered.append(True))
        with patch("sys.argv", ["lancer.pyw", "--system"]), \
                patch.dict(sys.modules, {"interface": SimpleNamespace(Application=lambda: app)}), \
                patch.dict(launcher["main"].__globals__, {
                    "PYTHON": Path(sys.executable),
                    "launch_local": lambda: self.fail("Le mode système ne doit pas ouvrir .venv"),
                    "startup_log": lambda detail: None,
                }):
            launcher["main"]()
        self.assertEqual(entered, [True])

    def test_mint_shortcut_uses_portable_system_launcher(self):
        text = (DIRECTORY / "Trace Série Mint.desktop").read_text(encoding="utf-8")
        command = next(line[5:] for line in text.splitlines() if line.startswith("Exec="))
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary) / "Dossier été ' $"
            folder.mkdir()
            shortcut = folder / "Trace Série Mint.desktop"
            shortcut.write_text(text, encoding="utf-8")
            (folder / "lancer-mint.sh").write_text("#!/bin/sh\nprintf 'MINT_OK'\n", encoding="utf-8")
            for location in (str(shortcut), shortcut.as_uri()):
                args = shlex.split(command)
                args[0] = sys.executable
                args[-1] = location
                result = subprocess.run(args, cwd=temporary, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, "MINT_OK")

    def test_supervises_success_and_silent_process_failures(self):
        class Window:
            def __init__(self):
                self.pending = []
                self.destroyed = False
                self.hidden = False

            def after(self, delay, callback):
                self.pending.append((time.monotonic() + delay / 1000, callback))

            def protocol(self, *args):
                pass

            def withdraw(self):
                self.hidden = True

            def destroy(self):
                self.destroyed = True

            def run(self):
                deadline = time.monotonic() + 5
                while not self.destroyed and time.monotonic() < deadline:
                    due = [item for item in self.pending if item[0] <= time.monotonic()]
                    self.pending = [item for item in self.pending if item not in due]
                    for _, callback in due:
                        callback()
                    time.sleep(.01)

        # os._exit représente une fin native que le try/except du lanceur ne voit pas.
        scripts = [
            ("import os; print('avant le crash', flush=True); os._exit(7)", False, True),
            ("print('sortie prématurée', flush=True)", False, True),
            ("from pathlib import Path; import sys,time; Path(sys.argv[-1]).write_text('ready'); time.sleep(.3)", True, False),
            ("from pathlib import Path; import sys,time,os; Path(sys.argv[-1]).write_text('ready'); time.sleep(.3); os._exit(9)", True, True),
        ]
        launcher = runpy.run_path(str(DIRECTORY / "lancer.pyw"))
        launch = launcher["launch_local"]
        for script, hidden, failure in scripts:
            with self.subTest(script=script), tempfile.TemporaryDirectory() as temporary:
                stub = Path(temporary) / "stub.pyw"
                stub.write_text(script, encoding="utf-8")
                window = Window()
                with patch.dict(launch.__globals__, {"DIRECTORY": Path(temporary), "PYTHON": Path(sys.executable), "__file__": str(stub)}), \
                        patch.dict(launch.__globals__, {"report_startup_error": lambda detail: reported.append(detail)}):
                    reported = []
                    launch(window)
                    window.run()
                self.assertTrue(window.destroyed)
                self.assertEqual(window.hidden, hidden)
                self.assertEqual(bool(reported), failure)
                # Journal désactivé : rien n'est écrit dans le dossier de l'application.
                self.assertFalse((Path(temporary) / "demarrage.log").exists())
                if "avant le crash" in script:
                    self.assertIn("avant le crash", reported[0])
                    self.assertIn("code 7", reported[0])

    def test_local_import_failure_does_not_restart_installation(self):
        launcher = runpy.run_path(str(DIRECTORY / "lancer.pyw"))
        original_import = builtins.__import__

        def without_matplotlib(name, *args, **kwargs):
            if name.startswith("matplotlib"):
                raise ImportError("lib absente")
            return original_import(name, *args, **kwargs)

        with patch("builtins.__import__", side_effect=without_matplotlib), \
                patch("sys.argv", ["lancer.pyw", "--local"]):
            with self.assertRaisesRegex(RuntimeError, "environnement .venv"):
                launcher["main"]()

    def test_desktop_launches_from_moved_folder_and_file_uri(self):
        desktop = (DIRECTORY / "Trace Série.desktop").read_text(encoding="utf-8")
        command = next(line[5:] for line in desktop.splitlines() if line.startswith("Exec="))
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary) / "Un autre dossier été ' $ test"
            folder.mkdir()
            shortcut = folder / "Trace Série.desktop"
            shortcut.write_text(desktop, encoding="utf-8")
            (folder / "voisin.py").write_text("VALUE = 'OK'\n", encoding="utf-8")
            (folder / "lancer.pyw").write_text("from voisin import VALUE\nprint(VALUE)\n", encoding="utf-8")
            for location in (str(shortcut), shortcut.as_uri()):
                arguments = shlex.split(command)
                self.assertEqual(arguments[-1], "%k")
                arguments[0] = sys.executable
                arguments[-1] = location
                result = subprocess.run(arguments, cwd=temporary, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "OK")

    def test_missing_tkinter_gets_installation_instructions(self):
        launcher = runpy.run_path(str(DIRECTORY / "lancer.pyw"))
        original_import = builtins.__import__

        def without_tkinter(name, *args, **kwargs):
            if name == "tkinter":
                raise ModuleNotFoundError("No module named tkinter")
            return original_import(name, *args, **kwargs)

        with patch("builtins.__import__", side_effect=without_tkinter):
            with self.assertRaisesRegex(RuntimeError, "sudo apt install python3-tk python3-venv") as caught:
                launcher["main"]()
            report = launcher["report_startup_error"]
            with tempfile.TemporaryDirectory() as temporary, \
                    patch.dict(report.__globals__, {"DIRECTORY": Path(temporary)}), \
                    patch("shutil.which", side_effect=lambda name: "/usr/bin/zenity" if name == "zenity" else None), \
                    patch("subprocess.run", return_value=subprocess.CompletedProcess([], 0)) as dialog, \
                    patch("sys.stderr", None):
                report(str(caught.exception))
                self.assertEqual(dialog.call_args.args[0][0], "zenity")
                self.assertIn("python3-tk", " ".join(dialog.call_args.args[0]))
                self.assertFalse((Path(temporary) / "demarrage.log").exists())

    def test_old_python_is_reported_before_importing_application(self):
        launcher = runpy.run_path(str(DIRECTORY / "lancer.pyw"))
        with patch.object(sys, "version_info", (3, 8, 0)):
            with self.assertRaisesRegex(RuntimeError, "Python 3.10"):
                launcher["main"]()


if __name__ == "__main__":
    unittest.main()
