"""Interface Tkinter de Trace Série."""

import math
import os
import threading
import time
import tkinter as tk
from collections import deque
from queue import Empty, Queue
from tkinter import colorchooser, filedialog, messagebox, ttk

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from serial.tools import list_ports

from acquisition import (DEMO_PORT, OPERATIONS, SerialReader, Session, calculation_unit, parse_section,
                         statistics)
from plot_view import PlotView, format_clock, format_duration
from preferences import Preferences
from time_view import PRESETS, TimeView


BG = "#eef1f5"
PANEL = "#ffffff"
BORDER = "#d7dce3"
TEXT = "#1f2933"
MUTED = "#6b7280"
ACCENT = "#2563eb"
ACCENT_DARK = "#1d4ed8"
SELECTED = "#dbeafe"
STATE_COLORS = {"connecting": "#d97706", "retrying": "#d97706", "connected": "#16a34a",
                "error": "#dc2626", "stopping": "#9ca3af"}
STATE_TEXT = {"connecting": "Connexion…", "retrying": "Reconnexion…", "connected": "Connecté",
              "error": "Erreur", "stopping": "Arrêt…"}
BAUDS = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]
DEMO_ENTRY = f"{DEMO_PORT} — signaux simulés"
# Sections imprimées par l'OPC ; seules les mesures sont tracées d'office.
SECTIONS = {"measurements": "Mesures", "regulators": "Régulateurs", "actuators": "Actionneurs",
            "outputs": "Sorties", "digital inputs": "Entrées TOR", "process control": "Process Control"}
REFRESH = 0.25


def thousands(number):
    return f"{number:,}".replace(",", " ")


def short_port(port):
    name = os.path.basename(port) or port
    return name if len(name) <= 22 else name[:21] + "…"


def section_label(section):
    return SECTIONS.get(section.casefold(), section)


def display_name(variable):
    """Nom affiché ; « Math 1 — A − B » devient « A − B » dans le groupe Calculs."""
    if variable.calculation and " — " in variable.name:
        return variable.name.split(" — ", 1)[1]
    return variable.name


def plotted_by_default(section):
    return section == "" or section.casefold() == "measurements"


def normalize_port(text):
    text = text.split(" — ")[0].strip()
    if text.casefold() == DEMO_PORT.casefold():
        return DEMO_PORT
    if text.startswith(("ttyACM", "ttyUSB")):
        text = "/dev/" + text
    elif text[:3].lower() in ("acm", "usb") and text[3:].isdigit():
        text = "/dev/tty" + text.upper()
    return text.upper() if os.name == "nt" else text


def same_device(a, b):
    if os.name == "posix" and DEMO_PORT not in (a, b):
        return os.path.realpath(a) == os.path.realpath(b)
    return a == b


def setup_style(root):
    style = ttk.Style(root)
    if "clam" in style.theme_names():
        style.theme_use("clam")
    root.configure(bg=BG)
    style.configure(".", background=BG, foreground=TEXT, bordercolor=BORDER,
                    lightcolor=BORDER, darkcolor=BORDER, focuscolor=ACCENT)
    for name, background in (("Panel", PANEL), ("Bar", PANEL), ("Status", BG)):
        style.configure(f"{name}.TFrame", background=background)
        style.configure(f"{name}.TLabel", background=background, foreground=TEXT)
        style.configure(f"{name}Muted.TLabel", background=background, foreground=MUTED)
        style.configure(f"{name}.TCheckbutton", background=background)
        style.map(f"{name}.TCheckbutton", background=[("active", background)])
    style.configure("PanelTitle.TLabel", background=PANEL, foreground=TEXT,
                    font=("TkDefaultFont", 11, "bold"))
    style.configure("TButton", padding=(10, 4), background=PANEL, bordercolor="#bfc6d1",
                    lightcolor=PANEL, darkcolor=PANEL)
    style.map("TButton", background=[("disabled", BG), ("active", "#f1f4f8")])
    style.configure("Accent.TButton", background=ACCENT, foreground="white",
                    bordercolor=ACCENT_DARK, lightcolor=ACCENT, darkcolor=ACCENT)
    style.map("Accent.TButton", background=[("disabled", "#9db6f2"), ("active", ACCENT_DARK)],
              foreground=[("disabled", "white")])
    style.configure("Live.TButton", foreground="#15803d", background=PANEL)
    style.configure("Frozen.TButton", foreground="white", background="#d97706",
                    bordercolor="#b45309", lightcolor="#d97706", darkcolor="#d97706")
    style.map("Frozen.TButton", background=[("active", "#b45309")])
    style.configure("Flat.TButton", padding=(4, 0), background=PANEL, bordercolor=PANEL,
                    lightcolor=PANEL, darkcolor=PANEL, foreground=MUTED)
    style.map("Flat.TButton", foreground=[("active", "#dc2626")], background=[("active", PANEL)])
    style.configure("PanelToggle.TButton", padding=(6, 3), background=PANEL, bordercolor=BORDER,
                    lightcolor=PANEL, darkcolor=PANEL, foreground=TEXT)
    style.map("PanelToggle.TButton", background=[("active", "#eef2f7")])
    style.configure("Toggle.TButton", padding=(8, 2), background=BG, bordercolor=BG,
                    lightcolor=BG, darkcolor=BG, foreground=MUTED)
    style.map("Toggle.TButton", background=[("active", "#e2e7ee")])
    # Boutons segmentés : un groupe de Radiobutton dessinés comme des boutons.
    style.layout("Segment.TRadiobutton", style.layout("Toolbutton"))
    style.configure("Segment.TRadiobutton", padding=(9, 3), background=PANEL, anchor="center")
    style.map("Segment.TRadiobutton", background=[("selected", ACCENT), ("active", "#eef2f7")],
              foreground=[("selected", "white")])
    style.configure("Chip.TFrame", background="#f1f4f8")
    style.configure("Chip.TLabel", background="#f1f4f8")
    style.configure("ChipFlat.TButton", padding=(4, 0), background="#f1f4f8", bordercolor="#f1f4f8",
                    lightcolor="#f1f4f8", darkcolor="#f1f4f8", foreground=MUTED)
    style.map("ChipFlat.TButton", foreground=[("active", "#dc2626")], background=[("active", "#f1f4f8")])
    style.configure("Treeview", rowheight=24, background=PANEL, fieldbackground=PANEL,
                    bordercolor=PANEL, indent=12)
    style.map("Treeview", background=[("selected", SELECTED)], foreground=[("selected", TEXT)])
    style.configure("Treeview.Heading", background="#f7f8fa", foreground=MUTED, relief="flat",
                    padding=(6, 3))
    style.map("TCombobox", fieldbackground=[("readonly", PANEL)], selectbackground=[("readonly", PANEL)],
              selectforeground=[("readonly", TEXT)])
    style.configure("TNotebook", background=PANEL, borderwidth=0)
    style.configure("TNotebook.Tab", padding=(10, 3))
    style.map("TNotebook.Tab", background=[("selected", PANEL)])


class Tooltip:
    def __init__(self, widget, text):
        self.widget, self.text, self.window = widget, text, None
        widget.bind("<Enter>", self.show, add="+")
        widget.bind("<Leave>", self.hide, add="+")

    def show(self, event=None):
        text = self.text() if callable(self.text) else self.text
        if not text or self.window:
            return
        self.window = tk.Toplevel(self.widget)
        self.window.wm_overrideredirect(True)
        self.window.wm_geometry(f"+{self.widget.winfo_rootx()}+{self.widget.winfo_rooty() + self.widget.winfo_height() + 4}")
        tk.Label(self.window, text=text, justify="left", bg="#1f2933", fg="white",
                 padx=8, pady=4, wraplength=420).pack()

    def hide(self, event=None):
        if self.window:
            self.window.destroy()
            self.window = None


class PortChip(ttk.Frame):
    """Pastille d'un appareil : état, nom et bouton de déconnexion."""

    def __init__(self, parent, port, baudrate, on_close, on_click):
        super().__init__(parent, style="Chip.TFrame", padding=(8, 2, 2, 2))
        self.port, self.baudrate = port, baudrate
        self.state, self.message, self.device = "connecting", "", ""
        self.dot = tk.Label(self, text="●", bg="#f1f4f8", font=("TkDefaultFont", 11))
        self.dot.pack(side="left")
        self.label = ttk.Label(self, text=short_port(port), style="Chip.TLabel")
        self.label.pack(side="left", padx=(3, 0))
        ttk.Button(self, text="✕", width=2, style="ChipFlat.TButton", command=on_close).pack(side="left")
        for widget in (self, self.dot, self.label):
            widget.bind("<Button-1>", lambda event: on_click())
        Tooltip(self.label, self.describe)
        self.set_state("connecting")

    def set_state(self, state, message="", device=None):
        self.state, self.message = state, message
        if device is not None:
            self.device = device
        self.dot.configure(fg=STATE_COLORS.get(state, MUTED))

    def describe(self):
        lines = [self.port, f"{STATE_TEXT.get(self.state, self.state)}"
                 + ("" if self.port == DEMO_PORT else f" · {self.baudrate} bauds")]
        if self.device:
            lines.append(f"Retrouvé sous {self.device}")
        if self.message:
            lines.append(self.message)
        lines.append("Cliquer pour voir ses sorties ; ✕ pour le déconnecter.")
        return "\n".join(lines)


class PortMonitor(ttk.Frame):
    """Dernières lignes d'un appareil ; le texte n'est rendu que s'il est visible."""

    MAX_LINES = 2000

    def __init__(self, parent, port):
        super().__init__(parent, style="Panel.TFrame")
        self.port = port
        self.lines = deque(maxlen=self.MAX_LINES)
        self.pending = []
        self.stale = True
        self.visible = False
        self.filter = tk.StringVar()
        self.hide_data = tk.BooleanVar(value=False)
        self.follow = tk.BooleanVar(value=True)
        controls = ttk.Frame(self, style="Panel.TFrame", padding=(0, 4))
        controls.pack(fill="x")
        ttk.Label(controls, text="Filtrer", style="PanelMuted.TLabel").pack(side="left")
        entry = ttk.Entry(controls, textvariable=self.filter, width=22)
        entry.pack(side="left", padx=(4, 12))
        self.filter.trace_add("write", lambda *args: self.invalidate())
        ttk.Checkbutton(controls, text="Masquer les mesures", variable=self.hide_data,
                        style="Panel.TCheckbutton", command=self.invalidate).pack(side="left")
        ttk.Checkbutton(controls, text="Défilement automatique", variable=self.follow,
                        style="Panel.TCheckbutton").pack(side="left", padx=12)
        ttk.Button(controls, text="Effacer", command=self.clear).pack(side="right")
        frame = ttk.Frame(self)
        frame.pack(fill="both", expand=True)
        self.text = tk.Text(frame, height=6, wrap="none", state="disabled", bg="#141c2b", fg="#dce5ef",
                            font=("TkFixedFont", 10), relief="flat", padx=10, pady=6,
                            selectbackground="#334766", highlightthickness=0)
        self.text.tag_configure("data", foreground="#dce5ef")
        self.text.tag_configure("message", foreground="#f2c66d")
        self.text.tag_configure("status", foreground="#ff8f8f")
        self.text.tag_configure("section", foreground="#6f7f96")
        vertical = ttk.Scrollbar(frame, command=self.text.yview)
        horizontal = ttk.Scrollbar(frame, orient="horizontal", command=self.text.xview)
        self.text.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        vertical.pack(side="right", fill="y")
        horizontal.pack(side="bottom", fill="x")
        self.text.pack(fill="both", expand=True)

    def add(self, items):
        self.lines.extend(items)
        if self.visible and not self.stale:
            self.pending.extend(items)
            if len(self.pending) > self.MAX_LINES:
                self.stale = True
        else:
            self.stale = True

    def invalidate(self):
        self.stale = True
        self.render()

    def matches(self, item):
        text, kind = item
        needle = self.filter.get()
        return not (self.hide_data.get() and kind == "data") and (not needle or needle.casefold() in text.casefold())

    def render(self):
        if not self.visible:
            return
        if self.stale:
            items, replace = list(self.lines), True
        elif self.pending:
            items, replace = self.pending, False
        else:
            return
        self.pending = []
        self.stale = False
        self.text.configure(state="normal")
        if replace:
            self.text.delete("1.0", "end")
        arguments = []
        for item in items:
            if self.matches(item):
                arguments += [item[0] + "\n", item[1]]
        if arguments:
            self.text.insert("end", *arguments)
        count = int(self.text.index("end-1c").split(".")[0]) - 1
        if count > self.MAX_LINES:
            self.text.delete("1.0", f"{count - self.MAX_LINES + 1}.0")
        if self.follow.get():
            self.text.see("end")
        self.text.configure(state="disabled")

    def clear(self):
        self.lines.clear()
        self.pending = []
        self.stale = True
        self.render()


class Dialog(tk.Toplevel):
    """Fenêtre modale simple, centrée sur la fenêtre principale."""

    def __init__(self, parent, title):
        super().__init__(parent)
        self.withdraw()
        self.title(title)
        self.transient(parent)
        self.resizable(False, False)
        self.configure(bg=PANEL)
        self.result = None
        self.body = ttk.Frame(self, style="Panel.TFrame", padding=16)
        self.body.pack(fill="both", expand=True)
        self.bind("<Escape>", lambda event: self.destroy())

    def buttons(self, ok_text, on_ok):
        row = ttk.Frame(self.body, style="Panel.TFrame")
        row.grid(row=99, column=0, columnspan=3, sticky="e", pady=(14, 0))
        ttk.Button(row, text="Annuler", command=self.destroy).pack(side="left", padx=4)
        self.ok = ttk.Button(row, text=ok_text, style="Accent.TButton", command=on_ok)
        self.ok.pack(side="left")
        self.bind("<Return>", lambda event: on_ok())

    def run(self):
        self.update_idletasks()
        parent = self.master
        x = parent.winfo_rootx() + (parent.winfo_width() - self.winfo_reqwidth()) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - self.winfo_reqheight()) // 3
        self.geometry(f"+{max(0, x)}+{max(0, y)}")
        self.deiconify()
        self.wait_visibility()
        self.grab_set()
        self.wait_window()
        return self.result


class CalculationDialog(Dialog):
    def __init__(self, parent, choices, left=None, right=None):
        super().__init__(parent, "Courbe calculée")
        self.choices = choices  # libellé -> variable
        labels = list(choices)
        by_key = {v.key: label for label, v in choices.items()}
        self.a = tk.StringVar(value=by_key.get(left, labels[0]))
        self.b = tk.StringVar(value=by_key.get(right, labels[1 if len(labels) > 1 else 0]))
        self.operation = tk.StringVar(value=next(iter(OPERATIONS)))
        self.preview = tk.StringVar()
        body = self.body
        ttk.Label(body, text="Nouvelle courbe calculée à partir de deux signaux.",
                  style="PanelMuted.TLabel").grid(row=0, column=0, columnspan=3, sticky="w", pady=(0, 10))
        for row, (text, widget) in enumerate((
                ("A", ttk.Combobox(body, textvariable=self.a, values=labels, state="readonly", width=38)),
                ("Opération", ttk.Combobox(body, textvariable=self.operation, values=list(OPERATIONS),
                                           state="readonly", width=38)),
                ("B", ttk.Combobox(body, textvariable=self.b, values=labels, state="readonly", width=38))),
                start=1):
            ttk.Label(body, text=text, style="Panel.TLabel").grid(row=row, column=0, sticky="w", padx=(0, 10), pady=3)
            widget.grid(row=row, column=1, sticky="ew", pady=3)
        ttk.Button(body, text="⇅ Inverser", command=self.swap).grid(row=1, column=2, rowspan=3, padx=(8, 0))
        self.preview_label = ttk.Label(body, textvariable=self.preview, style="Panel.TLabel", wraplength=420)
        self.preview_label.grid(row=4, column=0, columnspan=3, sticky="w", pady=(10, 0))
        for variable in (self.a, self.b, self.operation):
            variable.trace_add("write", lambda *args: self.update_preview())
        self.buttons("Ajouter la courbe", self.accept)
        self.update_preview()

    def swap(self):
        a, b = self.a.get(), self.b.get()
        self.a.set(b)
        self.b.set(a)

    def update_preview(self):
        a, b = self.choices[self.a.get()], self.choices[self.b.get()]
        operation = self.operation.get()
        try:
            if a is b:
                raise ValueError("Choisissez deux signaux différents.")
            unit = calculation_unit(a.unit, b.unit, operation)
        except ValueError as error:
            self.preview.set(str(error))
            self.preview_label.configure(foreground="#dc2626")
            self.ok.configure(state="disabled")
            return
        symbol = OPERATIONS[operation][0]
        self.preview.set(f"Résultat : {a.name} {symbol} {b.name}" + (f" ({unit})" if unit else ""))
        self.preview_label.configure(foreground=TEXT)
        self.ok.configure(state="normal")

    def accept(self):
        if str(self.ok["state"]) == "disabled":
            return
        self.result = (self.choices[self.a.get()].key, self.choices[self.b.get()].key, self.operation.get())
        self.destroy()


class RangeDialog(Dialog):
    def __init__(self, parent, title, low, high):
        super().__init__(parent, title)
        self.low = tk.StringVar(value=f"{low:.6g}")
        self.high = tk.StringVar(value=f"{high:.6g}")
        for row, (text, variable) in enumerate((("Minimum", self.low), ("Maximum", self.high))):
            ttk.Label(self.body, text=text, style="Panel.TLabel").grid(row=row, column=0, sticky="w", padx=(0, 10), pady=3)
            entry = ttk.Entry(self.body, textvariable=variable, width=14)
            entry.grid(row=row, column=1, pady=3)
            if row == 0:
                entry.focus_set()
                entry.select_range(0, "end")
        self.buttons("Appliquer", self.accept)

    def accept(self):
        try:
            low = float(self.low.get().replace(",", "."))
            high = float(self.high.get().replace(",", "."))
            if not (math.isfinite(low) and math.isfinite(high) and low < high):
                raise ValueError
        except ValueError:
            messagebox.showerror("Échelle", "Saisissez deux nombres avec Minimum < Maximum.", parent=self)
            return
        self.result = (low, high)
        self.destroy()


class Application(tk.Tk):
    def __init__(self, preferences=None):
        super().__init__()
        self.prefs = preferences if preferences is not None else Preferences()
        self.title("Trace Série")
        width = min(1366, max(900, self.winfo_screenwidth() - 60))
        height = min(820, max(560, self.winfo_screenheight() - 90))
        # Seule la taille est restaurée : la position dépend de l'écran branché.
        self.geometry(str(self.prefs.get("geometry") or f"{width}x{height}").split("+")[0])
        self.minsize(900, 560)
        setup_style(self)
        self.session = Session()
        self.session.defaults = self.signal_defaults
        self.events = Queue()
        self.readers = {}
        self.chips = {}
        self.monitors = {}
        self.rows = {}
        self.groups = {}
        self.row_text = {}
        self.swatches = {}
        self.changed = set()
        self.received_lines = 0
        self.last_refresh = 0.0
        self.dirty_plot = True
        self.closing = False
        self.exporting = False
        self.drag = None
        self.time_view = TimeView(self.prefs.get("window", 600.0))
        self.port = tk.StringVar(value=self.prefs.get("port", ""))
        self.baud = tk.StringVar(value=str(self.prefs.get("baud", 115200)))
        self.window = tk.StringVar(value=self.window_key(self.time_view.duration))
        self.x_mode = tk.StringVar(value=self.prefs.get("x_mode", "duree"))
        self.split_units = tk.BooleanVar(value=self.prefs.get("split_units", True))
        self.filter = tk.StringVar()
        self.status = tk.StringVar()
        self.readout = tk.StringVar()
        self.counter = tk.StringVar()
        self.monitor_visible = self.prefs.get("monitor", True)
        self.sidebar_visible = self.prefs.get("sidebar_visible", True)
        self.stats_keys = []
        self.stats_rows = {}
        self.stats_text = {}
        self.stats_scope = tk.StringVar(value=self.prefs.get("stats_scope", "vue"))
        self.stats_markers = tk.BooleanVar(value=self.prefs.get("stats_markers", True))
        self.stats_span = tk.StringVar()
        self.stats_message = None
        self._build()
        self.plot.y_limits = {unit: tuple(limits) for unit, limits in self.prefs.get("y_limits", {}).items()}
        self.apply_plot_options()
        self.set_status("Choisissez un port puis cliquez sur Connecter. Les signaux « Nom : valeur unité » "
                        "sont détectés automatiquement.")
        self.refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self.close)
        self.after(50, self.poll)

    # ----------------------------------------------------------- construction

    def _build(self):
        self._build_toolbar()
        status = ttk.Frame(self, style="Status.TFrame", padding=(12, 3))
        status.pack(side="bottom", fill="x")
        self.status_label = ttk.Label(status, textvariable=self.status, style="StatusMuted.TLabel")
        self.status_label.pack(side="left")
        self.monitor_button = ttk.Button(status, style="Toggle.TButton", command=self.toggle_monitor)
        self.monitor_button.pack(side="right")
        ttk.Label(status, textvariable=self.counter, style="StatusMuted.TLabel").pack(side="right", padx=12)

        self.main = tk.PanedWindow(self, orient="horizontal", sashwidth=6, bg=BG, bd=0,
                                   sashrelief="flat", opaqueresize=True)
        self.main.pack(fill="both", expand=True, padx=8, pady=(8, 0))
        self.sidebar = self._build_sidebar(self.main)
        self.main.add(self.sidebar, minsize=230, width=self.prefs.get("sidebar", 320), stretch="never")
        self.right = tk.PanedWindow(self.main, orient="vertical", sashwidth=6, bg=BG, bd=0,
                                    sashrelief="flat", opaqueresize=True)
        self.main.add(self.right, minsize=450, stretch="always")
        self.right.add(self._build_plot(self.right), minsize=220, stretch="always")
        self.monitor_area = self._build_monitor(self.right)
        self.right.add(self.monitor_area, minsize=110, height=self.prefs.get("monitor_height", 210),
                       stretch="never")
        self.show_monitor(self.monitor_visible)
        self.show_sidebar(self.sidebar_visible)

        self.bind_all("<Control-b>", lambda event: self.toggle_sidebar())
        self.bind_all("<Control-e>", lambda event: self.export())
        self.bind_all("<Control-s>", lambda event: self.save_image())
        self.bind_all("<End>", lambda event: self.go_live())

    def _build_toolbar(self):
        bar = ttk.Frame(self, style="Bar.TFrame", padding=(12, 8))
        bar.pack(fill="x")
        ttk.Label(bar, text="Port", style="Bar.TLabel").pack(side="left")
        self.port_box = ttk.Combobox(bar, textvariable=self.port, width=30, postcommand=self.refresh_ports)
        self.port_box.pack(side="left", padx=(6, 10))
        self.port_box.bind("<Return>", lambda event: self.connect())
        ttk.Label(bar, text="Bauds", style="Bar.TLabel").pack(side="left")
        ttk.Combobox(bar, textvariable=self.baud, width=8, values=BAUDS).pack(side="left", padx=(6, 10))
        ttk.Button(bar, text="Connecter", style="Accent.TButton", command=self.connect).pack(side="left")
        ttk.Separator(bar, orient="vertical").pack(side="left", fill="y", padx=12)
        self.chips_frame = ttk.Frame(bar, style="Bar.TFrame")
        self.chips_frame.pack(side="left", fill="y")
        self.no_device = ttk.Label(self.chips_frame, text="Aucun appareil connecté", style="BarMuted.TLabel")
        self.no_device.pack(side="left")
        self.image_button = ttk.Button(bar, text="Image…", command=self.save_image)
        self.image_button.pack(side="right")
        Tooltip(self.image_button, "Enregistrer le graphique en PNG, SVG ou PDF (Ctrl+S)")
        self.export_button = ttk.Button(bar, text="Exporter CSV…", command=self.export)
        self.export_button.pack(side="right", padx=6)
        Tooltip(self.export_button, "Exporter toutes les mesures reçues (Ctrl+E)")
        clear = ttk.Button(bar, text="Nouvelle acquisition", command=self.new_acquisition)
        clear.pack(side="right")
        Tooltip(clear, "Effacer les mesures et remettre le temps à zéro.\n"
                       "Les ports, signaux, couleurs et calculs sont conservés.")
        tk.Frame(self, height=1, bg=BORDER).pack(fill="x")

    def _build_sidebar(self, parent):
        sidebar = ttk.Frame(parent, style="Panel.TFrame", padding=(10, 10, 10, 8))
        header = ttk.Frame(sidebar, style="Panel.TFrame")
        header.pack(fill="x")
        ttk.Label(header, text="Signaux", style="PanelTitle.TLabel").pack(side="left")
        entry = ttk.Entry(header, textvariable=self.filter, width=14)
        entry.pack(side="right")
        ttk.Label(header, text="Filtrer", style="PanelMuted.TLabel").pack(side="right", padx=4)
        self.filter.trace_add("write", lambda *args: self.apply_filter())

        frame = ttk.Frame(sidebar, style="Panel.TFrame")
        frame.pack(fill="both", expand=True, pady=(8, 6))
        self.tree = ttk.Treeview(frame, columns=("value", "unit"), selectmode="extended")
        self.tree.heading("#0", text="Signal", anchor="w")
        self.tree.heading("value", text="Valeur", anchor="e")
        self.tree.heading("unit", text="Unité", anchor="w")
        self.tree.column("#0", width=150, minwidth=100, stretch=True)
        self.tree.column("value", width=74, minwidth=60, anchor="e", stretch=False)
        self.tree.column("unit", width=60, minwidth=40, anchor="w", stretch=False)
        self.tree.tag_configure("group", foreground=MUTED, font=("TkDefaultFont", 9, "bold"))
        self.tree.tag_configure("hidden", foreground="#9ca3af")
        self.tree.tag_configure("fault", foreground="#dc2626")
        scroll = ttk.Scrollbar(frame, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self.tree.pack(fill="both", expand=True)
        self.tree.bind("<Button-1>", self.on_tree_click)
        self.tree.bind("<Double-1>", self.on_tree_double)
        self.tree.bind("<space>", lambda event: self.toggle_plotted(self.selected_keys()))
        self.tree.bind("<Delete>", lambda event: self.delete_signals(self.selected_keys()))
        self.tree.bind("<Button-3>", self.tree_menu)
        self.tree.bind("<Button-2>", self.tree_menu)

        ttk.Button(sidebar, text="+ Courbe calculée…", command=self.add_calculation).pack(fill="x")
        ttk.Label(sidebar, style="PanelMuted.TLabel", wraplength=290, justify="left",
                  text="Pastille pleine : courbe affichée. Cliquez dessus pour l’afficher ou "
                       "la masquer. Clic droit : mesures, couleur, calcul, suppression.").pack(fill="x", pady=(8, 0))
        # La colonne des noms prend toute la largeur restante.
        self.tree.bind("<Configure>", lambda event: self.tree.column(
            "#0", width=max(100, event.width - 136)))
        return sidebar

    def _build_plot(self, parent):
        area = ttk.Frame(parent, style="Panel.TFrame")
        bar = ttk.Frame(area, style="Panel.TFrame", padding=(10, 8, 10, 0))
        bar.pack(fill="x")
        self.sidebar_button = ttk.Button(bar, style="PanelToggle.TButton", width=9, command=self.toggle_sidebar)
        self.sidebar_button.pack(side="left", padx=(0, 12))
        Tooltip(self.sidebar_button, "Masquer ou afficher la liste des signaux (Ctrl+B)")
        ttk.Label(bar, text="Fenêtre", style="PanelMuted.TLabel").pack(side="left", padx=(0, 6))
        for label, duration in PRESETS:
            ttk.Radiobutton(bar, text=label, value=self.window_key(duration), variable=self.window,
                            style="Segment.TRadiobutton", command=self.apply_preset).pack(side="left")
        ttk.Frame(bar, width=14, style="Panel.TFrame").pack(side="left")
        for label, value in (("Durée", "duree"), ("Heure", "heure")):
            button = ttk.Radiobutton(bar, text=label, value=value, variable=self.x_mode,
                                     style="Segment.TRadiobutton", command=self.apply_plot_options)
            button.pack(side="left")
            Tooltip(button, "Axe du temps : durée depuis le début, ou heure locale du PC.")
        split = ttk.Checkbutton(bar, text="Par unité", variable=self.split_units, style="Panel.TCheckbutton",
                                command=self.apply_plot_options)
        split.pack(side="left", padx=(14, 0))
        Tooltip(split, "Un graphe par unité (°C, %HR…), tous alignés sur le même temps.")
        self.live_button = ttk.Button(bar, command=self.toggle_live, width=11)
        self.live_button.pack(side="right")
        Tooltip(self.live_button, "En direct : l’axe suit les dernières mesures.\n"
                                  "Figé : la vue ne bouge plus, l’acquisition continue.\n"
                                  "Cliquez pour basculer ; Fin pour revenir au direct.")

        self.figure = Figure(figsize=(8, 4.4), dpi=100, layout="constrained", facecolor=PANEL)
        self.plot = PlotView(self.figure)
        self.canvas = FigureCanvasTkAgg(self.figure, master=area)
        self.time_scrollbar = ttk.Scrollbar(area, orient="horizontal", command=self.on_scrollbar)
        self.time_scrollbar.pack(side="bottom", fill="x", padx=10, pady=(0, 6))
        ttk.Label(area, textvariable=self.readout, style="PanelMuted.TLabel", anchor="w",
                  padding=(12, 0)).pack(side="bottom", fill="x")
        self.stats_frame = self._build_stats(area)
        widget = self.canvas.get_tk_widget()
        widget.configure(highlightthickness=0, bg=PANEL)
        widget.pack(fill="both", expand=True, padx=4)
        for name, handler in (("scroll_event", self.on_scroll), ("button_press_event", self.on_press),
                              ("button_release_event", self.on_release),
                              ("motion_notify_event", self.on_motion),
                              ("figure_leave_event", lambda event: self.show_hint()),
                              ("resize_event", lambda event: self.mark_plot())):
            self.canvas.mpl_connect(name, handler)
        self.show_hint()
        return area

    def _build_stats(self, area):
        """Bandeau de mesures façon oscilloscope, affiché dès qu'un signal y est ajouté."""
        frame = ttk.Frame(area, style="Panel.TFrame", padding=(10, 4, 10, 2))
        header = ttk.Frame(frame, style="Panel.TFrame")
        header.pack(fill="x", pady=(0, 4))
        ttk.Label(header, text="Mesures", style="PanelTitle.TLabel").pack(side="left", padx=(0, 10))
        for label, value in (("Vue affichée", "vue"), ("Toute l’acquisition", "tout")):
            ttk.Radiobutton(header, text=label, value=value, variable=self.stats_scope,
                            style="Segment.TRadiobutton", command=self.draw_plot).pack(side="left")
        ttk.Label(header, textvariable=self.stats_span, style="PanelMuted.TLabel").pack(side="left", padx=10)
        close = ttk.Button(header, text="✕", width=2, style="Flat.TButton", command=self.clear_stats)
        close.pack(side="right")
        Tooltip(close, "Fermer les mesures")
        markers = ttk.Checkbutton(header, text="Repères sur le graphe", variable=self.stats_markers,
                                  style="Panel.TCheckbutton", command=self.draw_plot)
        markers.pack(side="right", padx=8)
        Tooltip(markers, "Moyenne en tirets, minimum et maximum en pointillés,\n"
                         "dans la couleur de chaque courbe affichée.")
        columns = (("mean", "Moyenne"), ("median", "Médiane"), ("min", "Min"), ("max", "Max"),
                   ("pp", "Crête-crête"), ("std", "Écart-type"), ("count", "Points"))
        self.stats_tree = ttk.Treeview(frame, columns=[c for c, _ in columns], height=1, selectmode="extended")
        self.stats_tree.heading("#0", text="Signal", anchor="w")
        self.stats_tree.column("#0", width=180, minwidth=120, stretch=True)
        for column, title in columns:
            self.stats_tree.heading(column, text=title, anchor="e")
            self.stats_tree.column(column, width={"count": 72, "pp": 112}.get(column, 100), minwidth=60,
                                   anchor="e", stretch=False)
        self.stats_tree.pack(fill="x")
        self.stats_tree.bind("<Delete>", lambda event: self.remove_stats(self.selected_stats()))
        self.stats_tree.bind("<Button-3>", self.stats_menu)
        self.stats_tree.bind("<Button-2>", self.stats_menu)
        self.stats_tree.bind("<<TreeviewSelect>>", lambda event: self.describe_stats())
        return frame

    def _build_monitor(self, parent):
        area = ttk.Frame(parent, style="Panel.TFrame", padding=(10, 6, 10, 6))
        self.notebook = ttk.Notebook(area)
        self.notebook.bind("<<NotebookTabChanged>>", lambda event: self.update_monitor_visibility())
        self.monitor_placeholder = ttk.Label(area, style="PanelMuted.TLabel",
                                             text="Moniteur série : connectez un appareil pour voir ses lignes reçues.")
        self.monitor_placeholder.pack(anchor="w", pady=8)
        return area

    # --------------------------------------------------------------- appareils

    def refresh_ports(self):
        try:
            ports = sorted(list_ports.comports(), key=lambda info: info.device)
        except OSError as error:
            self.set_status(f"Recherche des ports impossible : {error}")
            ports = []
        values = []
        for info in ports:
            description = info.description if info.description and info.description != "n/a" else ""
            values.append(f"{info.device} — {description}" if description and description != info.device
                          else info.device)
        self.port_box["values"] = values + [DEMO_ENTRY]
        if not self.port.get() and values:
            self.port.set(values[0])

    def connect(self):
        port = normalize_port(self.port.get())
        if not port:
            messagebox.showerror("Port série", "Choisissez ou saisissez un port série.", parent=self)
            return
        try:
            baud = int(self.baud.get())
            if baud <= 0:
                raise ValueError
        except ValueError:
            messagebox.showerror("Débit", "Le débit doit être un entier positif.", parent=self)
            return
        existing = next((p for p in self.readers if same_device(p, port)), None)
        if existing:
            self.select_monitor(existing)
            self.set_status(f"{short_port(existing)} est déjà connecté.")
            return
        if self.session.origin is None:
            self.session.start()
            self.time_view.reset()
        reader = SerialReader(port, baud, self.events, demo=port == DEMO_PORT)
        self.readers[port] = reader
        self.no_device.pack_forget()
        chip = PortChip(self.chips_frame, port, baud, lambda: self.disconnect(port),
                        lambda: self.select_monitor(port))
        chip.pack(side="left", padx=(0, 6))
        self.chips[port] = chip
        monitor = PortMonitor(self.notebook, port)
        self.monitors[port] = monitor
        self.notebook.add(monitor, text=short_port(port))
        self.monitor_placeholder.pack_forget()
        self.notebook.pack(fill="both", expand=True, pady=(4, 0))
        self.notebook.select(monitor)
        self.prefs["port"] = self.port.get() if port != DEMO_PORT else self.prefs.get("port", "")
        self.prefs["baud"] = baud
        self.refresh_group_labels()
        reader.start()

    def disconnect(self, port):
        reader = self.readers.get(port)
        if reader is None:
            return
        reader.stop()
        self.chips[port].set_state("stopping")

    def port_done(self, port):
        self.readers.pop(port, None)
        chip = self.chips.pop(port, None)
        if chip:
            chip.destroy()
        monitor = self.monitors.pop(port, None)
        if monitor:
            self.notebook.forget(monitor)
            monitor.destroy()
        if not self.readers:
            self.no_device.pack(side="left")
            self.notebook.pack_forget()
            self.monitor_placeholder.pack(anchor="w", pady=8)
        if not self.closing:
            self.set_status(f"{short_port(port)} déconnecté. Ses mesures restent disponibles.")
        self.refresh_group_labels()

    def set_port_state(self, port, state, message):
        chip = self.chips.get(port)
        if chip is None or chip.state == "stopping":
            return
        device = message if state == "connected" else None
        chip.set_state(state, "" if state == "connected" else message, device)
        name = short_port(port)
        if state == "connected":
            self.set_status(f"{name} connecté" + (f" (retrouvé sous {message})" if message else "") + ".")
        elif state == "retrying" and message:
            self.set_status(f"{name} : {message} — nouvelle tentative chaque seconde.")
        elif state == "error":
            self.set_status(f"{name} : {message}")

    def select_monitor(self, port):
        monitor = self.monitors.get(port)
        if monitor:
            self.show_monitor(True)
            self.notebook.select(monitor)

    # ----------------------------------------------------------------- signaux

    def signal_defaults(self, reading):
        saved = self.prefs.get("signals", {}).get(reading.name)
        if saved:
            return saved.get("color"), saved.get("plotted", True)
        return None, plotted_by_default(reading.section)

    def remember_signal(self, variable):
        if variable.calculation is None:
            self.prefs.setdefault("signals", {})[variable.name] = {
                "color": variable.color, "plotted": variable.plotted}

    def group_key(self, variable):
        return "Math" if variable.calculation else f"{variable.source}\x00{variable.section}"

    def group_text(self, variable):
        if variable.calculation:
            return "Calculs"
        text = section_label(variable.section) if variable.section else "Signaux"
        if len({v.source for v in self.session.variables.values() if not v.calculation}) > 1:
            text = f"{short_port(variable.source)} · {text}"
        return text

    def refresh_group_labels(self):
        for variable in self.session.variables.values():
            group = self.groups.get(self.group_key(variable))
            if group:
                self.tree.item(group, text=self.group_text(variable))
        self.plot.signature = None
        self.mark_plot()

    def swatch(self, color, plotted):
        key = (color, plotted)
        if key not in self.swatches:
            image = tk.PhotoImage(width=16, height=14)
            image.put(color, to=(2, 1, 14, 13))
            if not plotted:
                image.put(PANEL, to=(4, 3, 12, 11))
            self.swatches[key] = image
        return self.swatches[key]

    def add_rows(self):
        for key, variable in self.session.variables.items():
            if key in self.rows:
                continue
            group_key = self.group_key(variable)
            group = self.groups.get(group_key)
            if group is None:
                group = self.tree.insert("", "end", text=self.group_text(variable), open=True, tags=("group",))
                self.groups[group_key] = group
            self.rows[key] = self.tree.insert(group, "end", text=" " + display_name(variable),
                                              values=("—", variable.unit))
            self.update_row(variable)
            if variable.plotted:
                self.plot.signature = None
        self.refresh_group_labels()
        self.apply_filter()

    def update_row(self, variable):
        row = self.rows.get(variable.key)
        if row is None:
            return
        tags = ("fault",) if variable.status else () if variable.plotted else ("hidden",)
        self.tree.item(row, image=self.swatch(variable.color, variable.plotted), tags=tags)
        self.update_value(variable)

    def update_value(self, variable):
        row = self.rows.get(variable.key)
        if row is None:
            return
        last = variable.series.last()
        if variable.status:
            text = variable.status
        elif last is None or not math.isfinite(last[1]):
            text = "—"
        else:
            text = f"{last[1]:.6g}"
        if self.row_text.get(row) != text:
            self.row_text[row] = text
            self.tree.set(row, "value", text)
            faulty = "fault" in self.tree.item(row, "tags")
            if faulty != bool(variable.status):
                self.update_row(variable)

    def apply_filter(self):
        needle = self.filter.get().casefold()
        for group_key, group in self.groups.items():
            children = [(key, row) for key, row in self.rows.items()
                        if self.group_key(self.session.variables[key]) == group_key]
            visible = 0
            for index, (key, row) in enumerate(children):
                if not needle or needle in display_name(self.session.variables[key]).casefold():
                    self.tree.move(row, group, index)
                    visible += 1
                else:
                    self.tree.detach(row)
            if visible:
                self.tree.move(group, "", list(self.groups).index(group_key))
            else:
                self.tree.detach(group)

    def selected_keys(self):
        keys = []
        by_row = {row: key for key, row in self.rows.items()}
        for row in self.tree.selection():
            if row in by_row:
                keys.append(by_row[row])
            else:
                keys += [by_row[child] for child in self.tree.get_children(row) if child in by_row]
        return keys

    def on_tree_click(self, event):
        row = self.tree.identify_row(event.y)
        if row in self.row_keys() and "image" in self.tree.identify_element(event.x, event.y):
            self.toggle_plotted([self.row_keys()[row]])
            return "break"
        return None

    def on_tree_double(self, event):
        row = self.tree.identify_row(event.y)
        if row in self.row_keys():
            self.toggle_plotted([self.row_keys()[row]])
            return "break"
        return None

    def row_keys(self):
        return {row: key for key, row in self.rows.items()}

    def toggle_plotted(self, keys, value=None):
        if not keys:
            return
        variables = [self.session.variables[key] for key in keys]
        if value is None:
            value = not all(v.plotted for v in variables)
        for variable in variables:
            variable.plotted = value
            self.update_row(variable)
            self.remember_signal(variable)
        self.draw_plot()

    def plot_only(self, keys):
        for key, variable in self.session.variables.items():
            if variable.plotted != (key in keys):
                variable.plotted = key in keys
                self.update_row(variable)
                self.remember_signal(variable)
        self.draw_plot()

    def toggle_separate(self, key):
        self.plot.separate ^= {key}
        variable = self.session.variables[key]
        if not variable.plotted:
            self.toggle_plotted([key], True)
        self.draw_plot()

    def choose_color(self, key):
        variable = self.session.variables[key]
        color = colorchooser.askcolor(variable.color, parent=self, title=f"Couleur de {variable.name}")[1]
        if color:
            variable.color = color
            self.update_row(variable)
            if key in self.stats_rows:
                self.stats_tree.item(self.stats_rows[key], image=self.swatch(color, True))
            self.remember_signal(variable)
            self.draw_plot()

    def delete_signals(self, keys):
        keys = [key for key in keys if key in self.session.variables]
        if not keys:
            return
        dependents = {d for key in keys for d in self.session.dependents(key)} - set(keys)
        names = ", ".join(display_name(self.session.variables[key]) for key in keys[:4]) + ("…" if len(keys) > 4 else "")
        message = f"Supprimer {names} et ses mesures ?"
        if dependents:
            message += f"\n\n{len(dependents)} courbe(s) calculée(s) qui en dépendent seront aussi supprimées."
        if any(not self.session.variables[key].calculation for key in keys):
            message += "\n\nUn signal encore reçu réapparaîtra automatiquement."
        if not messagebox.askyesno("Supprimer", message, parent=self):
            return
        for key in keys:
            if key not in self.session.variables:
                continue
            removed_keys = self.session.remove_variable(key)
            self.remove_stats(removed_keys)
            for removed in removed_keys:
                self.plot.separate.discard(removed)
                row = self.rows.pop(removed, None)
                if row:
                    self.row_text.pop(row, None)
                    self.tree.delete(row)
        for group_key, group in list(self.groups.items()):
            if not any(self.group_key(v) == group_key for v in self.session.variables.values()):
                self.tree.delete(group)
                del self.groups[group_key]
        self.draw_plot()

    def calculation_choices(self):
        choices = {}
        several = len({v.source for v in self.session.variables.values() if not v.calculation}) > 1
        for variable in self.session.variables.values():
            label = display_name(variable) + (f" ({variable.unit})" if variable.unit else "")
            if several and not variable.calculation:
                label += f" [{short_port(variable.source)}]"
            while label in choices:
                label += " "
            choices[label] = variable
        return choices

    def add_calculation(self, left=None, right=None):
        choices = self.calculation_choices()
        if len(choices) < 2:
            messagebox.showinfo("Courbe calculée", "Il faut au moins deux signaux reçus pour créer un calcul.",
                                parent=self)
            return
        selected = self.selected_keys()
        if left is None and selected:
            left = selected[0]
            right = selected[1] if len(selected) > 1 else None
        result = CalculationDialog(self, choices, left, right).run()
        if not result:
            return
        try:
            variable = self.session.add_calculation(*result)
        except ValueError as error:
            messagebox.showerror("Courbe calculée", str(error), parent=self)
            return
        self.add_rows()
        self.tree.selection_set(self.rows[variable.key])
        self.tree.see(self.rows[variable.key])
        self.set_status(f"Courbe ajoutée : {variable.name}")
        self.draw_plot()

    def tree_menu(self, event):
        row = self.tree.identify_row(event.y)
        if not row:
            return
        if row not in self.tree.selection():
            self.tree.selection_set(row)
        keys = self.selected_keys()
        if not keys:
            return
        menu = tk.Menu(self, tearoff=False)
        plotted = all(self.session.variables[key].plotted for key in keys)
        menu.add_command(label="Masquer" if plotted else "Afficher",
                         command=lambda: self.toggle_plotted(keys, not plotted))
        menu.add_command(label="Afficher seulement la sélection", command=lambda: self.plot_only(set(keys)))
        if len(keys) == 1:
            menu.add_command(label="Couleur…", command=lambda: self.choose_color(keys[0]))
            separate = tk.BooleanVar(value=keys[0] in self.plot.separate)
            menu.add_checkbutton(label="Graphe séparé", variable=separate,
                                 command=lambda: self.toggle_separate(keys[0]))
            menu.separate = separate  # Garde la variable Tk vivante pendant l'affichage du menu.
        menu.add_separator()
        menu.add_command(label="Mesures (moyenne, médiane, min, max…)", command=lambda: self.add_stats(keys))
        menu.add_command(label="Courbe calculée…",
                         command=lambda: self.add_calculation(keys[0], keys[1] if len(keys) > 1 else None))
        menu.add_separator()
        menu.add_command(label="Supprimer", command=lambda: self.delete_signals(keys))
        menu.tk_popup(event.x_root, event.y_root)

    # --------------------------------------------------------------- graphique

    @staticmethod
    def window_key(duration):
        return "tout" if duration is None else f"{duration:g}"

    def apply_preset(self):
        duration = next(d for label, d in PRESETS if self.window_key(d) == self.window.get())
        self.time_view.set_duration(duration)
        self.time_view.follow = True
        self.draw_plot()

    def apply_plot_options(self):
        self.plot.x_mode = self.x_mode.get()
        self.plot.split_units = self.split_units.get()
        self.draw_plot()

    def mark_plot(self):
        self.dirty_plot = True

    def plot_labels(self, variables):
        several = len({v.source for v in self.session.variables.values() if not v.calculation}) > 1
        return {v.key: display_name(v) + (f" ({v.unit})" if v.unit else "")
                + (f" [{short_port(v.source)}]" if several and not v.calculation else "")
                for v in variables}

    def draw_plot(self):
        self.dirty_plot = False
        plotted = [v for v in self.session.variables.values() if v.plotted]
        self.plot.started_at = self.session.started_at
        self.plot.empty_message = ("Aucun signal tracé : cliquez sur la pastille d’un signal à gauche."
                                   if self.session.variables else
                                   "Connectez un appareil : ses signaux apparaîtront à gauche.")
        self.plot.sync(plotted, self.plot_labels(plotted))
        latest = self.session.latest_time()
        start, end = self.time_view.bounds(latest)
        self.update_stats(start, end)
        width = max(300, self.canvas.get_tk_widget().winfo_width())
        self.plot.update(plotted, start, end, 2 * width)
        self.time_scrollbar.set(*self.time_view.fractions(latest))
        self.canvas.draw_idle()
        self.update_live_button()
        self.window.set(self.window_key(self.time_view.duration)
                        if self.time_view.duration in [d for _, d in PRESETS] else "")

    def update_live_button(self):
        if self.time_view.follow:
            self.live_button.configure(text="● En direct", style="Live.TButton")
        else:
            self.live_button.configure(text="⏸ Figé", style="Frozen.TButton")

    def toggle_live(self):
        if self.time_view.follow:
            self.time_view.freeze(self.session.latest_time())
        else:
            self.time_view.follow = True
        self.draw_plot()

    def go_live(self):
        self.time_view.follow = True
        self.draw_plot()

    def on_scrollbar(self, *args):
        self.time_view.scroll(self.session.latest_time(), *args)
        self.draw_plot()

    @staticmethod
    def modifiers(event):
        names = getattr(event, "modifiers", None)
        if names is not None:
            return {"ctrl" if name == "ctrl" else name for name in names}
        state = getattr(getattr(event, "guiEvent", None), "state", 0) or 0
        return {name for name, mask in (("shift", 0x1), ("ctrl", 0x4)) if state & mask}

    def on_scroll(self, event):
        if event.inaxes is None or event.xdata is None:
            return
        modifiers = self.modifiers(event)
        latest = self.session.latest_time()
        factor = 0.8 if event.button == "up" else 1.25
        if "ctrl" in modifiers:
            group = self.plot.group_at(event.inaxes)
            low, high = event.inaxes.get_ylim()
            center = event.ydata
            self.plot.y_limits[group] = (center - (center - low) * factor, center + (high - center) * factor)
        elif "shift" in modifiers:
            start, end = self.time_view.bounds(latest)
            self.time_view.pan((end - start) * (0.1 if event.button == "down" else -0.1), latest)
        else:
            self.time_view.zoom(factor, event.xdata, latest)
        self.draw_plot()

    def on_press(self, event):
        if event.inaxes is None:
            return
        if event.button == 3:
            self.after_idle(lambda: self.plot_menu(event.inaxes))
        elif event.button == 1 and event.dblclick:
            self.plot.y_limits.pop(self.plot.group_at(event.inaxes), None)
            self.draw_plot()
        elif event.button == 1:
            start, end = self.time_view.bounds(self.session.latest_time())
            self.drag = (event.x, start, end, event.inaxes.bbox.width)
            self.canvas.get_tk_widget().configure(cursor="fleur")

    def on_release(self, event):
        if self.drag:
            self.drag = None
            self.canvas.get_tk_widget().configure(cursor="")

    def on_motion(self, event):
        if self.drag:
            x, start, end, width = self.drag
            delta = -(event.x - x) * (end - start) / max(width, 1)
            self.time_view.show(start + delta, end + delta, self.session.latest_time())
            self.draw_plot()
        if event.inaxes is None or event.xdata is None:
            self.show_hint()
            return
        parts = [self.format_time(event.xdata)]
        for variable in self.session.variables.values():
            if variable.plotted:
                value = variable.series.value_at(event.xdata)
                if value is not None and math.isfinite(value):
                    parts.append(f"{display_name(variable)} {value:.5g}{' ' + variable.unit if variable.unit else ''}")
        self.readout.set("   ·   ".join(parts[:8]))

    def format_time(self, seconds):
        if self.plot.x_mode == "heure" and self.session.started_at:
            return format_clock(self.session.started_at, seconds, 0.1)
        return format_duration(seconds, 0.1)

    def show_hint(self):
        self.readout.set("Molette : zoom · Glisser : déplacer · Ctrl+molette : zoom vertical · "
                         "Double-clic : échelle auto · Clic droit : options")

    def plot_menu(self, ax):
        group = self.plot.group_at(ax)
        if group is None:
            return
        name = "" if group == PlotView.ALL else f" ({group or 'sans unité'})"
        menu = tk.Menu(self, tearoff=False)
        menu.add_command(label=f"Échelle verticale automatique{name}",
                         command=lambda: (self.plot.y_limits.pop(group, None), self.draw_plot()))
        menu.add_command(label=f"Fixer l’échelle verticale{name}…", command=lambda: self.ask_range(ax, group))
        keys = [key for key, line in self.plot.lines.items() if line.axes is ax]
        if keys:
            menu.add_command(label="Mesures des courbes de ce graphe", command=lambda: self.add_stats(keys))
        menu.add_separator()
        menu.add_command(label="Revenir au direct", command=self.go_live)
        menu.add_command(label="Afficher tout l’historique", command=lambda: (
            self.time_view.set_duration(None), self.draw_plot()))
        menu.add_separator()
        menu.add_command(label="Enregistrer l’image…", command=self.save_image)
        x, y = self.winfo_pointerxy()
        menu.tk_popup(x, y)

    def ask_range(self, ax, group):
        result = RangeDialog(self, "Échelle verticale", *ax.get_ylim()).run()
        if result:
            self.plot.y_limits[group] = result
            self.draw_plot()

    # ----------------------------------------------------------------- mesures

    def add_stats(self, keys):
        for key in keys:
            if key in self.stats_keys or key not in self.session.variables:
                continue
            variable = self.session.variables[key]
            self.stats_keys.append(key)
            label = display_name(variable) + (f" ({variable.unit})" if variable.unit else "")
            self.stats_rows[key] = self.stats_tree.insert(
                "", "end", text=" " + label, image=self.swatch(variable.color, True))
        if self.stats_keys:
            self.stats_tree.configure(height=min(len(self.stats_keys), 5))
            self.stats_frame.pack(side="bottom", fill="x", before=self.canvas.get_tk_widget())
        self.draw_plot()

    def remove_stats(self, keys):
        for key in keys:
            if key in self.stats_keys:
                self.stats_keys.remove(key)
                row = self.stats_rows.pop(key)
                self.stats_text.pop(row, None)
                self.stats_tree.delete(row)
        if self.stats_keys:
            self.stats_tree.configure(height=min(len(self.stats_keys), 5))
        else:
            self.stats_frame.pack_forget()
        self.draw_plot()

    def clear_stats(self):
        self.remove_stats(list(self.stats_keys))

    def selected_stats(self):
        by_row = {row: key for key, row in self.stats_rows.items()}
        return [by_row[row] for row in self.stats_tree.selection() if row in by_row]

    def stats_menu(self, event):
        row = self.stats_tree.identify_row(event.y)
        if row and row not in self.stats_tree.selection():
            self.stats_tree.selection_set(row)
        menu = tk.Menu(self, tearoff=False)
        if self.selected_stats():
            menu.add_command(label="Retirer des mesures", command=lambda: self.remove_stats(self.selected_stats()))
        menu.add_command(label="Tout retirer", command=self.clear_stats)
        menu.tk_popup(event.x_root, event.y_root)

    def stats_bounds(self, start, end):
        return (None, None) if self.stats_scope.get() == "tout" else (start, end)

    def update_stats(self, start, end):
        """Recalcule les mesures ; appelé à chaque tracé, donc aussi après un zoom."""
        self.plot.markers = {}
        if not self.stats_keys:
            return
        low, high = self.stats_bounds(start, end)
        for key in self.stats_keys:
            variable = self.session.variables[key]
            result = statistics(variable.times, variable.values, low, high)
            if result is None:
                cells = ("—",) * 6 + ("0",)
            else:
                cells = tuple(f"{value:.4g}" for value in (result.mean, result.median, result.minimum,
                                                            result.maximum, result.peak_to_peak, result.std))
                cells += (thousands(result.count),)
                if self.stats_markers.get() and variable.plotted:
                    self.plot.markers[key] = (result.mean, result.minimum, result.maximum)
            row = self.stats_rows[key]
            if self.stats_text.get(row) != cells:
                self.stats_text[row] = cells
                self.stats_tree.item(row, values=cells)
        if low is None:
            self.stats_span.set(f"sur {format_duration(self.session.latest_time())}")
        else:
            self.stats_span.set(f"sur {format_duration(end - start)} affichées")
        # Les instants du min et du max suivent les mesures tant que la barre d'état les montre.
        if self.stats_message is not None and self.status.get() == self.stats_message:
            self.describe_stats()

    def describe_stats(self):
        keys = self.selected_stats()
        if not keys:
            self.stats_message = None
            return
        variable = self.session.variables[keys[0]]
        low, high = self.stats_bounds(*self.time_view.bounds(self.session.latest_time()))
        result = statistics(variable.times, variable.values, low, high)
        if result is None:
            return
        unit = f" {variable.unit}" if variable.unit else ""
        self.stats_message = (f"{display_name(variable)} : min {result.minimum:.6g}{unit} à "
                              f"{self.format_time(result.minimum_time)}, max {result.maximum:.6g}{unit} à "
                              f"{self.format_time(result.maximum_time)}")
        self.set_status(self.stats_message)

    # ---------------------------------------------------------------- panneaux

    def toggle_sidebar(self):
        self.show_sidebar(not self.sidebar_visible)

    def show_sidebar(self, visible):
        if not visible and self.sidebar_visible:
            self.remember_sidebar_width()
        self.sidebar_visible = visible
        self.main.paneconfigure(self.sidebar, hide=not visible)
        if visible:
            self.main.paneconfigure(self.sidebar, width=self.prefs.get("sidebar", 320))
        self.sidebar_button.configure(text="◂ Signaux" if visible else "▸ Signaux")
        self.mark_plot()

    def remember_sidebar_width(self):
        try:
            width = self.main.sash_coord(0)[0]
        except tk.TclError:
            return
        if 200 <= width <= 700:
            self.prefs["sidebar"] = width

    # ---------------------------------------------------------------- moniteur

    def toggle_monitor(self):
        self.show_monitor(not self.monitor_visible)

    def show_monitor(self, visible):
        self.monitor_visible = visible
        self.right.paneconfigure(self.monitor_area, hide=not visible)
        self.monitor_button.configure(text=("▾ Masquer" if visible else "▴ Afficher") + " le moniteur série")
        self.update_monitor_visibility()

    def update_monitor_visibility(self):
        selected = self.notebook.select() if self.monitors else ""
        for monitor in self.monitors.values():
            monitor.visible = self.monitor_visible and str(monitor) == selected
            monitor.render()

    # ----------------------------------------------------------------- boucle

    def poll(self):
        deadline = time.monotonic() + 0.04
        changed = self.changed
        while time.monotonic() < deadline:
            try:
                source, kind, payload = self.events.get_nowait()
            except Empty:
                break
            if kind == "lines":
                items = []
                for received_at, line, reading in payload:
                    if reading is None:
                        items.append((line, "section" if parse_section(line) is not None else "message"))
                        continue
                    keys = self.session.ingest_reading(received_at, reading, source)
                    changed.update(keys)
                    known = (source, reading.name) in self.session.variables
                    items.append((line, "data" if reading.value is not None else "status" if known else "message"))
                self.received_lines += len(payload)
                monitor = self.monitors.get(source)
                if monitor:
                    monitor.add(items)
            elif kind == "state":
                self.set_port_state(source, *payload)
            elif kind == "done":
                self.port_done(source)
            elif kind == "export":
                self.export_done(*payload)
        now = time.monotonic()
        if now - self.last_refresh >= REFRESH:
            self.last_refresh = now
            self.refresh(changed)
        if self.closing and (not self.readers or now > self.close_deadline):
            self.destroy()
            return
        self.after(50, self.poll)

    def refresh(self, changed):
        if any(key not in self.rows for key in changed if key in self.session.variables):
            self.add_rows()
        for key in changed:
            variable = self.session.variables.get(key)
            if variable is not None:
                self.update_value(variable)
        if changed:
            self.dirty_plot = True
            changed.clear()
        for monitor in self.monitors.values():
            monitor.render()
        if self.session.started_at is not None:
            duration = format_duration(self.session.latest_time())
            self.counter.set(f"⏱ {duration}   ·   {thousands(self.received_lines)} lignes   ·   "
                             f"{thousands(self.session.sample_count)} points   ·   "
                             f"{len(self.session.variables)} signaux")
        if self.dirty_plot and not self.drag:
            self.draw_plot()

    def set_status(self, text):
        self.status.set(text)

    # ------------------------------------------------------------- fichiers

    def new_acquisition(self):
        if self.session.unsaved and not messagebox.askyesno(
                "Nouvelle acquisition", "Les mesures non exportées seront effacées. Continuer ?", parent=self):
            return
        self.session.start()
        self.time_view.reset()
        self.received_lines = 0
        for variable in self.session.variables.values():
            self.update_row(variable)
        self.set_status("Nouvelle acquisition : temps remis à zéro.")
        self.draw_plot()

    def ask_export_path(self):
        if not self.session.sample_count:
            messagebox.showinfo("Export CSV", "Aucune mesure à exporter pour l’instant.", parent=self)
            return None
        return filedialog.asksaveasfilename(
            parent=self, title="Exporter les mesures", defaultextension=".csv",
            filetypes=[("Données CSV", "*.csv")],
            initialfile="mesures_" + time.strftime("%Y%m%d_%H%M%S") + ".csv") or None

    def export(self):
        """Écrit le CSV dans un thread ; l'interface reste fluide."""
        if self.exporting:
            return
        path = self.ask_export_path()
        if not path:
            return
        snapshot = self.session.export_snapshot()
        self.exporting = True
        self.export_button.configure(state="disabled")
        self.set_status("Export CSV en cours…")

        def write():
            try:
                snapshot.write(path)
                self.events.put(("", "export", (True, path, snapshot.sample_count)))
            except (OSError, UnicodeError) as error:
                self.events.put(("", "export", (False, str(error), 0)))

        threading.Thread(target=write, daemon=True).start()

    def export_done(self, success, detail, count):
        self.exporting = False
        self.export_button.configure(state="normal")
        if success:
            self.session.exported_count = max(self.session.exported_count, count)
            self.set_status(f"CSV exporté : {thousands(count)} points → {detail}")
        else:
            messagebox.showerror("Export CSV", detail, parent=self)

    def export_now(self):
        path = self.ask_export_path()
        if not path:
            return False
        try:
            self.session.export_csv(path)
        except (OSError, UnicodeError) as error:
            messagebox.showerror("Export CSV", str(error), parent=self)
            return False
        return True

    def save_image(self):
        path = filedialog.asksaveasfilename(
            parent=self, title="Enregistrer le graphique", defaultextension=".png",
            filetypes=[("Image PNG", "*.png"), ("Image SVG", "*.svg"), ("Document PDF", "*.pdf")],
            initialfile="courbes_" + time.strftime("%Y%m%d_%H%M%S") + ".png")
        if not path:
            return
        try:
            self.figure.savefig(path, dpi=150, facecolor="white")
        except (OSError, ValueError) as error:
            messagebox.showerror("Enregistrer l’image", str(error), parent=self)
            return
        self.set_status(f"Image enregistrée : {path}")

    def save_preferences(self):
        # Fenêtre réduite ou masquée : sa taille n'a pas de sens.
        if self.winfo_width() >= 600 and self.winfo_height() >= 400:
            self.prefs["geometry"] = f"{self.winfo_width()}x{self.winfo_height()}"
        self.prefs["window"] = self.time_view.duration
        self.prefs["x_mode"] = self.x_mode.get()
        self.prefs["split_units"] = self.split_units.get()
        self.prefs["monitor"] = self.monitor_visible
        self.prefs["sidebar_visible"] = self.sidebar_visible
        self.prefs["stats_scope"] = self.stats_scope.get()
        self.prefs["stats_markers"] = self.stats_markers.get()
        self.prefs["y_limits"] = {unit: list(limits) for unit, limits in self.plot.y_limits.items()}
        try:
            if self.sidebar_visible:
                self.remember_sidebar_width()
            if self.monitor_visible and self.right.winfo_height() > 300:
                self.prefs["monitor_height"] = self.right.winfo_height() - self.right.sash_coord(0)[1]
        except tk.TclError:
            pass
        for variable in self.session.variables.values():
            self.remember_signal(variable)
        self.prefs.save()

    def close(self):
        if self.closing:
            return
        if self.session.unsaved:
            answer = messagebox.askyesnocancel("Fermer Trace Série", "Exporter les mesures avant de fermer ?",
                                               parent=self)
            if answer is None or (answer and not self.export_now()):
                return
        self.save_preferences()
        self.closing = True
        # Laisse les threads fermer proprement leurs ports, sans attendre indéfiniment.
        self.close_deadline = time.monotonic() + 2
        for reader in self.readers.values():
            reader.stop()


def main():
    Application().mainloop()


if __name__ == "__main__":
    main()
