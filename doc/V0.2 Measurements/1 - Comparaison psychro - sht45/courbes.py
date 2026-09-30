"""Trace A/D et G/L de la première feuille ODS, sans modifier le fichier.

Installation : python -m pip install numpy matplotlib
Utilisation : python courbes.py "OPC v0.2, RH"
Export seul : python courbes.py "OPC v0.2, RH" --sortie courbes.png --sans-affichage
"""

import argparse
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from zipfile import ZipFile

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import numpy as np


TABLE = "{urn:oasis:names:tc:opendocument:xmlns:table:1.0}"
OFFICE = "{urn:oasis:names:tc:opendocument:xmlns:office:1.0}"


def secondes(texte):
    """Convertit une durée ODS, par exemple PT10H49M59S, en secondes."""
    match = re.fullmatch(r"PT(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?", texte)
    if not match:
        raise ValueError(f"Heure ODS non reconnue : {texte}")
    h, m, s = (float(x or 0) for x in match.groups())
    return 3600 * h + 60 * m + s


def lire_series(fichier):
    # Lecture directe du format ODS : fonctionne aussi sans extension .ods.
    with ZipFile(fichier) as archive:
        if archive.read("mimetype").decode().strip() != "application/vnd.oasis.opendocument.spreadsheet":
            raise ValueError("Le fichier doit être un classeur ODS.")
        racine = ET.fromstring(archive.read("content.xml"))
    feuille = racine.find(f"{OFFICE}body/{OFFICE}spreadsheet/{TABLE}table")
    if feuille is None:
        raise ValueError("Aucune feuille trouvée.")
    series = [[], []]
    for ligne in feuille.iter(f"{TABLE}table-row"):
        cellules = []
        for cellule in ligne:
            if cellule.tag not in (f"{TABLE}table-cell", f"{TABLE}covered-table-cell"):
                continue
            repetitions = int(cellule.get(f"{TABLE}number-columns-repeated", "1"))
            cellules.extend([cellule] * min(repetitions, 12 - len(cellules)))
            if len(cellules) == 12:
                break
        for serie, (temps_col, valeur_col) in zip(series, [(0, 3), (6, 11)]):
            if len(cellules) <= valeur_col:
                continue
            temps = cellules[temps_col].get(f"{OFFICE}time-value")
            valeur = cellules[valeur_col].get(f"{OFFICE}value")
            if temps is None or valeur is None:
                continue  # Ignore les cellules vides et les éventuels en-têtes.
            point = (secondes(temps), float(valeur))
            serie.extend([point] * int(ligne.get(f"{TABLE}number-rows-repeated", "1")))
    resultat = []
    for numero, serie in enumerate(series, 1):
        donnees = np.asarray(serie, dtype=float)
        if len(donnees) < 2 or not np.isfinite(donnees).all():
            raise ValueError(f"Série {numero} : données insuffisantes ou non finies.")
        temps, valeurs = donnees.T
        if np.any(np.diff(temps) <= 0):
            raise ValueError(f"Série {numero} : heures dupliquées ou non croissantes.")
        resultat.append((temps, valeurs))
    return feuille.get(f"{TABLE}name"), resultat


def difference_absolue(t1, y1, t2, y2):
    debut, fin = max(t1[0], t2[0]), min(t1[-1], t2[-1])
    if debut >= fin:
        raise ValueError("Les séries n'ont pas de période commune suffisante.")
    # Union des instants mesurés dans la période commune, sans extrapolation.
    temps = np.union1d(t1[(t1 >= debut) & (t1 <= fin)],
                       t2[(t2 >= debut) & (t2 <= fin)])
    ecart = np.interp(temps, t1, y1) - np.interp(temps, t2, y2)
    # Ajoute les croisements pour conserver les minima à zéro de |écart|.
    indices = np.flatnonzero(ecart[:-1] * ecart[1:] < 0)
    croisements = temps[indices] - ecart[indices] * np.diff(temps)[indices] / np.diff(ecart)[indices]
    temps = np.union1d(temps, croisements)
    return temps, np.abs(np.interp(temps, t1, y1) - np.interp(temps, t2, y2))


def heure(valeur, position=None):
    h, reste = divmod(round(valeur), 3600)
    m, s = divmod(reste, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fichier", type=Path)
    parser.add_argument("--sortie", type=Path, help="Image à enregistrer (PNG, PDF…).")
    parser.add_argument("--sans-affichage", action="store_true")
    args = parser.parse_args()
    if args.sortie and args.sortie.resolve() == args.fichier.resolve():
        parser.error("L'image de sortie ne peut pas remplacer le fichier source.")
    nom, ((t1, y1), (t2, y2)) = lire_series(args.fichier)
    temps, ecart = difference_absolue(t1, y1, t2, y2)
    fig, (ax, diff) = plt.subplots(2, 1, sharex=True, figsize=(11, 7), layout="constrained")
    ax.plot(t1, y1, label="Série 1 — A / D", linewidth=1)
    ax.plot(t2, y2, label="Série 2 — G / L", linewidth=1)
    ax.set(title=f"{nom} — comparaison des séries", ylabel="Valeur")
    ax.legend()
    diff.plot(temps, ecart, color="tab:red", linewidth=1, label="|Série 1 − Série 2| (interpolation linéaire)")
    diff.set(xlabel="Heure", ylabel="Différence absolue", ylim=(0, None))
    diff.legend()
    diff.xaxis.set_major_formatter(FuncFormatter(heure))
    for axe in (ax, diff):
        axe.grid(alpha=0.25)
    print(f"Feuille : {nom}")
    for i, t in enumerate((t1, t2), 1):
        print(f"Série {i} : {len(t)} points, de {heure(t[0])} à {heure(t[-1])}")
    if args.sortie:
        fig.savefig(args.sortie, dpi=160)
    if not args.sans_affichage:
        plt.show()
    plt.close(fig)


if __name__ == "__main__":
    main()
