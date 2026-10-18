#!/usr/bin/env python3
"""Lanceur graphique ; installe les dépendances localement si nécessaire."""

import os
from datetime import datetime
from pathlib import Path
from queue import Empty, Queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback


DIRECTORY = Path(__file__).resolve().parent
ENVIRONMENT = DIRECTORY / ".venv"
PYTHON = ENVIRONMENT / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
PROCESS_OPTIONS = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
LOG_LIMIT = 256 * 1024


def startup_log(detail):
    """Écrit dès le lancement, pas seulement après une exception Python.

    Journal désactivé pour l'instant : décommenter le corps pour retrouver
    demarrage.log. Sans journal, renvoie None.
    """
    # diagnostic = (f"\nDate : {datetime.now().isoformat()}\nPython : {sys.executable}\n"
    #               f"Version : {sys.version}\nDossier : {DIRECTORY}\n{detail}\n")
    # for candidate in (DIRECTORY / "demarrage.log", Path(tempfile.gettempdir()) / "traceserie-demarrage.log"):
    #     try:
    #         # Le journal grandit à chaque lancement : on ne garde que la fin.
    #         if candidate.exists() and candidate.stat().st_size > LOG_LIMIT:
    #             tail = candidate.read_bytes()[-LOG_LIMIT // 2:]
    #             candidate.write_bytes(tail[tail.find(b"\n") + 1:])
    #         with candidate.open("a", encoding="utf-8") as stream:
    #             stream.write(diagnostic)
    #         return candidate
    #     except OSError:
    #         continue
    return None


def report_startup_error(detail):
    """Garde une trace et affiche une erreur même si Tkinter est absent."""
    log_path = startup_log(detail)
    message = detail[-4000:] + (f"\n\nDiagnostic enregistré : {log_path}" if log_path else "")
    if sys.stderr is not None:
        print(message, file=sys.stderr)
    root = None
    try:
        import tkinter as tk
        from tkinter import messagebox
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("Trace Série — démarrage impossible", message, parent=root)
        return
    except Exception:
        pass
    finally:
        if root is not None:
            root.destroy()
    # Les boîtes de dialogue du bureau ne dépendent pas de Python/Tkinter.
    for executable, arguments in (
        ("zenity", ["--error", "--no-markup", "--title=Trace Série — démarrage impossible", "--text=" + message]),
        ("xmessage", ["-center", message]),
    ):
        if shutil.which(executable):
            try:
                if subprocess.run([executable, *arguments]).returncode == 0:
                    return
            except OSError:
                pass
    if log_path is not None and shutil.which("xdg-open"):
        subprocess.Popen(["xdg-open", str(log_path)])


def process_failure(returncode, ready, cancelled=False):
    if cancelled:
        return None
    if returncode != 0:
        return f"Le processus de l’application s’est arrêté avec le code {returncode}."
    if not ready:
        return "Le processus s’est terminé sans ouvrir la fenêtre de l’application."
    return None


def launch_local(root=None):
    """Surveille le processus, y compris les plantages natifs sans traceback."""
    own_window = root is None
    if own_window:
        root = tk.Tk()
        root.title("Trace Série — Démarrage")
        ttk.Label(root, text="Ouverture de Trace Série…", padding=24).pack()
    log_path = startup_log("Démarrage du Python local : " + str(PYTHON))
    temporary = tempfile.TemporaryDirectory(prefix="traceserie-start-")
    ready_path = Path(temporary.name) / "ready"
    # Sans journal, la sortie du processus reste lisible en cas d'échec.
    output_path = log_path or Path(temporary.name) / "sortie.log"
    try:
        with output_path.open("a", encoding="utf-8") as stream:
            process = subprocess.Popen(
                [str(PYTHON), "-u", "-X", "faulthandler", str(Path(__file__).resolve()),
                 "--local", "--managed", str(ready_path)],
                cwd=DIRECTORY, stdout=stream, stderr=subprocess.STDOUT, **PROCESS_OPTIONS)
    except Exception:
        temporary.cleanup()
        root.destroy()
        raise
    started = time.monotonic()
    ready = False
    cancelled = False
    timed_out = False

    def terminate():
        if process.poll() is None:
            process.terminate()
            root.after(2000, lambda: process.kill() if process.poll() is None else None)

    def cancel():
        nonlocal cancelled
        cancelled = True
        terminate()

    root.protocol("WM_DELETE_WINDOW", cancel)

    def check():
        nonlocal ready, timed_out
        if not ready and ready_path.exists():
            ready = True
            startup_log("Fenêtre de l’application ouverte.")
            root.withdraw()
        returncode = process.poll()
        if returncode is None:
            if not ready and not cancelled and not timed_out and time.monotonic() - started > 90:
                timed_out = True
                terminate()
            root.after(150, check)
            return
        failure = process_failure(returncode, ready, cancelled)
        if timed_out and not cancelled:
            failure = "L’application n’a pas confirmé son ouverture après 90 secondes."
        details = ""
        if failure:
            try:
                with output_path.open("rb") as stream:
                    stream.seek(max(0, output_path.stat().st_size - 5000))
                    details = stream.read().decode("utf-8", errors="replace")
            except OSError:
                pass
        temporary.cleanup()
        root.destroy()
        if failure:
            report_startup_error(failure + "\n\n" + details)

    root.after(150, check)
    if own_window:
        root.mainloop()


def install_dialog():
    root = tk.Tk()
    root.report_callback_exception = lambda kind, error, tb: report_startup_error(
        "".join(traceback.format_exception(kind, error, tb)))
    root.title("Trace Série — Installation")
    root.geometry("610x350")
    root.minsize(550, 320)
    frame = ttk.Frame(root, padding=22)
    frame.pack(fill="both", expand=True)
    ttk.Label(frame, text="Bienvenue dans Trace Série", font=("TkDefaultFont", 16, "bold")).pack(anchor="w")
    ttk.Label(frame, text="Matplotlib et pySerial sont nécessaires. Le bouton ci-dessous\n"
                         "les installe dans le dossier .venv à côté de l’application.\n"
                         "Une connexion Internet est nécessaire au premier lancement.",
              padding=(0, 15)).pack(anchor="w")
    status = tk.StringVar(value="Votre installation Python système sera conservée.")
    ttk.Label(frame, textvariable=status, wraplength=540).pack(anchor="w", pady=8)
    progress = ttk.Progressbar(frame, mode="indeterminate")
    progress.pack(fill="x", pady=10)
    events = Queue()

    def work():
        log = []
        created = not ENVIRONMENT.exists()
        try:
            commands = [
                [sys.executable, "-m", "venv", str(ENVIRONMENT)],
                [str(PYTHON), "-m", "pip", "install", "--disable-pip-version-check",
                 "-r", str(DIRECTORY / "requirements.txt")],
            ]
            for command in commands:
                result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        text=True, errors="replace", **PROCESS_OPTIONS)
                log.append(result.stdout)
                if result.returncode:
                    raise RuntimeError(result.stdout[-5000:])
            events.put((True, ""))
        except Exception as error:
            # Un .venv partiel ferait échouer tous les lancements suivants.
            if created:
                shutil.rmtree(ENVIRONMENT, ignore_errors=True)
            events.put((False, str(error)))
        # Journal d'installation désactivé pour l'instant.
        # finally:
        #     try:
        #         (DIRECTORY / "installation.log").write_text("\n".join(log), encoding="utf-8")
        #     except OSError:
        #         pass

    def check():
        try:
            success, detail = events.get_nowait()
        except Empty:
            root.after(150, check)
            return
        progress.stop()
        root.protocol("WM_DELETE_WINDOW", root.destroy)
        if success:
            status.set("Installation terminée. Ouverture de l’application…")
            startup_log("Installation terminée ; lancement de l’application.")
            launch_local(root)
        else:
            status.set("Installation incomplète. Vérifiez Internet et les droits du dossier.")
            button.configure(state="normal")
            messagebox.showerror("Installation", detail + "\n\nSur Debian/Ubuntu, le paquet python3-venv doit être installé.", parent=root)

    def install():
        button.configure(state="disabled")
        root.protocol("WM_DELETE_WINDOW", lambda: None)
        status.set("Installation en cours… Cette étape peut prendre quelques minutes.")
        startup_log("Installation des dépendances dans " + str(ENVIRONMENT))
        progress.start()
        threading.Thread(target=work, daemon=True).start()
        root.after(150, check)

    button = ttk.Button(frame, text="Installer et ouvrir l’application", command=install)
    button.pack(anchor="w", pady=8)
    root.mainloop()


def main():
    if sys.version_info < (3, 10):
        raise RuntimeError("Trace Série nécessite Python 3.10 ou supérieur.\n"
                           "La version détectée est " + sys.version.split()[0] + ".")
    global tk, messagebox, ttk
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except ImportError as error:
        raise RuntimeError("Tkinter n’est pas installé pour ce Python.\n\n"
                           "Sous Linux Mint, installez les paquets python3-tk et python3-venv\n"
                           "dans le gestionnaire de paquets, ou exécutez :\n"
                           "sudo apt install python3-tk python3-venv\n\n"
                           "Puis relancez Trace Série.") from error
    # Réutilise l'environnement créé au premier lancement.
    if "--system" not in sys.argv and "--local" not in sys.argv and PYTHON.exists():
        launch_local()
        return
    try:
        import matplotlib.backends.backend_tkagg
        import serial.tools.list_ports
    except ImportError as error:
        if "--system" in sys.argv:
            raise RuntimeError("Le mode Linux Mint utilise les bibliothèques du système.\n"
                               "Installez-les avec cette commande :\n"
                               "sudo apt install python3-tk python3-numpy python3-matplotlib python3-serial") from error
        if "--local" in sys.argv:
            raise RuntimeError("L’environnement .venv ne peut pas charger les dépendances installées.\n"
                               "Supprimez le dossier .venv à côté de l’application puis relancez :\n"
                               "l’installation sera proposée à nouveau.") from error
        install_dialog()
        return
    startup_log("Import de l’interface graphique.")
    from interface import Application
    app = Application()
    def report_error(error_type, error, tb):
        # Erreur pendant l'utilisation : la fenêtre reste ouverte et les données aussi.
        detail = "".join(traceback.format_exception(error_type, error, tb))
        log_path = startup_log("Erreur dans l’interface :\n" + detail)
        messagebox.showerror("Trace Série — erreur interne",
                             f"{error_type.__name__} : {error}\n\nL’acquisition continue."
                             + (f"\nDétail enregistré dans {log_path}" if log_path else ""), parent=app)
    app.report_callback_exception = report_error
    if "--managed" in sys.argv:
        ready_path = Path(sys.argv[sys.argv.index("--managed") + 1])
        app.after_idle(lambda: ready_path.write_text("ready", encoding="utf-8"))
    else:
        startup_log("Fenêtre de l’application créée.")
    app.mainloop()


if __name__ == "__main__":
    try:
        startup_log("Lanceur démarré. Arguments : " + repr(sys.argv[1:]))
        main()
    except Exception:
        if "--managed" in sys.argv:
            # Le parent affiche l'erreur et capture cette sortie dans le journal.
            traceback.print_exc()
        else:
            report_startup_error(traceback.format_exc())
        sys.exit(1)
