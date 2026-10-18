"""Graphique matplotlib : un axe par unité, décimation et axe du temps."""

import math
from datetime import timedelta

import numpy as np
from matplotlib.ticker import Formatter, Locator


TEXT = "#1f2933"
MUTED = "#6b7280"
GRID = "#e5e7eb"
STEPS = [0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800,
         3600, 7200, 10800, 21600, 43200, 86400, 172800, 604800]


def tick_step(span, target=7):
    for step in STEPS:
        if span / step <= target:
            return step
    return STEPS[-1] * math.ceil(span / target / STEPS[-1])


def format_duration(seconds, step=1.0):
    """h:mm:ss, avec un dixième quand le pas est inférieur à la seconde."""
    sign = "-" if seconds < 0 else ""
    seconds = abs(seconds)
    if step < 1:
        tenths = round(seconds * 10)
        whole, tenth = divmod(tenths, 10)
        hours, rest = divmod(whole, 3600)
        return f"{sign}{hours}:{rest // 60:02d}:{rest % 60:02d}.{tenth}"
    hours, rest = divmod(round(seconds), 3600)
    return f"{sign}{hours}:{rest // 60:02d}:{rest % 60:02d}"


def format_clock(started_at, seconds, step=1.0):
    moment = started_at + timedelta(seconds=seconds)
    if step < 1:
        return f"{moment:%H:%M:%S}.{moment.microsecond // 100000}"
    if step >= 86400 or (step >= 3600 and moment.hour == 0 and moment.minute == 0):
        return f"{moment:%d/%m}"
    return f"{moment:%H:%M}" if step >= 60 else f"{moment:%H:%M:%S}"


def seconds_since_midnight(started_at):
    if started_at is None:
        return 0.0
    midnight = started_at.replace(hour=0, minute=0, second=0, microsecond=0)
    return (started_at - midnight).total_seconds()


class TimeLocator(Locator):
    """Graduations sur des durées rondes, alignées sur l'heure locale si besoin."""

    def __init__(self, view):
        self.view = view

    def __call__(self):
        return self.tick_values(*self.axis.get_view_interval())

    def tick_values(self, vmin, vmax):
        if vmax < vmin:
            vmin, vmax = vmax, vmin
        step = tick_step(max(vmax - vmin, 1e-9))
        offset = seconds_since_midnight(self.view.started_at) if self.view.x_mode == "heure" else 0.0
        first = math.ceil((vmin + offset) / step) * step - offset
        count = min(1000, int((vmax - first) / step) + 1)
        return [first + index * step for index in range(max(0, count))]


class TimeFormatter(Formatter):
    def __init__(self, view):
        self.view = view

    def __call__(self, x, pos=None):
        vmin, vmax = self.axis.get_view_interval()
        step = tick_step(max(abs(vmax - vmin), 1e-9))
        if self.view.x_mode == "heure" and self.view.started_at is not None:
            return format_clock(self.view.started_at, x, step)
        return format_duration(x, step)


def decimate(times, values, start, end, max_points):
    """Points visibles, réduits à un couple min/max par colonne de pixels.

    Garde un point de chaque côté de la vue pour ne pas couper les segments au
    bord. Un trou (NaN) dans un paquet reste visible comme une coupure.
    """
    first = max(0, int(np.searchsorted(times, start, side="left")) - 1)
    last = min(len(times), int(np.searchsorted(times, end, side="right")) + 1)
    t, v = times[first:last], values[first:last]
    count = len(t)
    if count <= max_points:
        return t.copy(), v.copy()
    buckets = max(1, max_points // 2)
    edges = np.linspace(0, count, buckets + 1).astype(np.intp)
    starts = edges[:-1]
    low = np.fmin.reduceat(v, starts)
    high = np.fmax.reduceat(v, starts)
    high[np.add.reduceat(np.isnan(v), starts) > 0] = np.nan
    x = np.empty(2 * buckets)
    y = np.empty(2 * buckets)
    x[0::2], x[1::2] = t[starts], t[edges[1:] - 1]
    y[0::2], y[1::2] = low, high
    return x, y


def auto_limits(arrays):
    finite = [a[np.isfinite(a)] for a in arrays]
    finite = [a for a in finite if a.size]
    if not finite:
        return 0.0, 1.0
    low = min(float(a.min()) for a in finite)
    high = max(float(a.max()) for a in finite)
    if high == low:
        pad = max(abs(high) * 0.05, 0.5)
    else:
        pad = (high - low) * 0.08
    return low - pad, high + pad


class PlotView:
    """Construit les axes (un par unité) et met à jour les courbes."""

    ALL = "*"

    def __init__(self, figure):
        self.figure = figure
        self.split_units = True
        self.x_mode = "duree"
        self.started_at = None
        self.y_limits = {}
        self.separate = set()
        # Repères des mesures : clé -> (moyenne, min, max), tracés dans la couleur du signal.
        self.markers = {}
        self.marker_lines = {}
        self.axes = {}
        self.lines = {}
        self.signature = None
        self.empty_message = "Aucun signal tracé"

    def group(self, variable):
        if variable.key in self.separate:
            return f"{variable.unit}|{variable.name}"
        return variable.unit if self.split_units else self.ALL

    def sync(self, variables, labels):
        """Reconstruit la figure si les courbes ou leur regroupement ont changé."""
        groups = list(dict.fromkeys(self.group(v) for v in variables))
        signature = (tuple(groups), tuple((v.key, v.color, labels[v.key], self.group(v)) for v in variables),
                     self.x_mode, self.empty_message)
        if signature == self.signature:
            return False
        self.signature = signature
        self.figure.clear()
        self.marker_lines.clear()
        self.axes.clear()
        self.lines.clear()
        axes = self.figure.subplots(max(1, len(groups)), 1, sharex=True, squeeze=False)[:, 0]
        for ax, group in zip(axes, groups or [None]):
            self._style(ax)
            if group is None:
                ax.text(0.5, 0.5, self.empty_message, transform=ax.transAxes,
                        ha="center", va="center", color=MUTED, fontsize=11)
                ax.set_yticks([])
                self.axes[self.ALL] = ax
                continue
            self.axes[group] = ax
            if group != self.ALL:
                ax.set_ylabel(group.split("|")[0] or "sans unité", color=MUTED)
            members = [v for v in variables if self.group(v) == group]
            for variable in members:
                line, = ax.plot([], [], color=variable.color, linewidth=1.4, label=labels[variable.key])
                self.lines[variable.key] = line
            # Légende au-dessus de l'axe : elle ne masque jamais les courbes.
            ax.legend(loc="lower left", bbox_to_anchor=(0, 1), fontsize=8, frameon=False,
                      ncols=min(5, len(members)), handlelength=1.4, borderaxespad=0.2)
        bottom = axes[-1]
        bottom.xaxis.set_major_locator(TimeLocator(self))
        bottom.xaxis.set_major_formatter(TimeFormatter(self))
        bottom.set_xlabel("Heure locale" if self.x_mode == "heure" else "Durée depuis le début (h:mm:ss)",
                          color=MUTED)
        return True

    @staticmethod
    def _style(ax):
        ax.set_facecolor("white")
        ax.grid(True, color=GRID, linewidth=0.8)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color("#c4cad3")
        ax.tick_params(colors=MUTED, labelcolor=TEXT, labelsize=9)

    def update(self, variables, start, end, max_points):
        data = {}
        for variable in variables:
            line = self.lines.get(variable.key)
            if line is None:
                continue
            x, y = decimate(variable.times, variable.values, start, end, max_points)
            line.set_data(x, y)
            data.setdefault(self.group(variable), []).append(y)
        self._update_markers(variables)
        for group, ax in self.axes.items():
            ax.set_xlim(start, end)
            limits = self.y_limits.get(group)
            ax.set_ylim(*(limits if limits else auto_limits(data.get(group, []))))

    def _update_markers(self, variables):
        colors = {v.key: v.color for v in variables}
        for key in list(self.marker_lines):
            if key not in self.markers or key not in self.lines:
                for line in self.marker_lines.pop(key):
                    line.remove()
        for key, levels in self.markers.items():
            line = self.lines.get(key)
            if line is None:
                continue
            if key not in self.marker_lines:
                self.marker_lines[key] = [
                    line.axes.axhline(0, color=colors[key], linewidth=1, alpha=0.8,
                                      linestyle=style, zorder=1)
                    for style in ("--", ":", ":")]
            for marker, level in zip(self.marker_lines[key], levels):
                marker.set_ydata([level, level])

    def group_at(self, ax):
        return next((group for group, candidate in self.axes.items() if candidate is ax), None)
