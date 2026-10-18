#!/usr/bin/env python3
"""Affiche et journalise les lignes reçues depuis un port série."""

from __future__ import annotations

import os
import select
import sys
import termios
import tty
from datetime import datetime
from pathlib import Path

BAUDRATE = 115200
VITESSES = {
    1200: termios.B1200,
    2400: termios.B2400,
    4800: termios.B4800,
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
    230400: termios.B230400,
}


def lister_ports() -> list[str]:
    """Retourne les ports série USB détectés, triés par nom."""
    noms: set[str] = set()

    # Les deux familles les plus courantes de convertisseurs série USB.
    for motif in ("ttyUSB*", "ttyACM*"):
        noms.update(chemin.name for chemin in Path("/dev").glob(motif))

    # Détecte également les pilotes USB utilisant un autre nom de périphérique.
    for chemin_tty in Path("/sys/class/tty").glob("tty*"):
        try:
            chemin_materiel = str((chemin_tty / "device").resolve()).lower()
        except OSError:
            continue
        if "/usb" in chemin_materiel and Path("/dev", chemin_tty.name).exists():
            noms.add(chemin_tty.name)

    return [f"/dev/{nom}" for nom in sorted(noms)]


def choisir_port(ports: list[str]) -> str:
    """Affiche les ports et demande à l'utilisateur d'en choisir un."""
    print("Ports série USB disponibles :")
    for numero, port in enumerate(ports, start=1):
        print(f"  {numero}. {port}")

    while True:
        try:
            choix = input("Sélectionnez un port (numéro) : ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            raise SystemExit(0)

        if choix.isdigit() and 1 <= int(choix) <= len(ports):
            return ports[int(choix) - 1]
        print(f"Choix invalide : entrez un nombre de 1 à {len(ports)}.")


def nom_fichier_log(port: str) -> Path:
    """Construit PORT-DATE-HEURE.log dans le répertoire courant."""
    port_sans_chemin = os.path.basename(port)
    date_heure = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    return Path(f"{port_sans_chemin}-{date_heure}.log")


def enregistrer_ligne(fichier, donnees: bytes) -> None:
    """Affiche et enregistre une ligne reçue, précédée de son horodatage."""
    texte = donnees.rstrip(b"\r\n").decode("utf-8", errors="replace")
    ligne = f"{datetime.now().strftime('%H:%M:%S')};{texte}"
    print(ligne, flush=True)
    fichier.write(ligne + "\n")
    fichier.flush()


def ouvrir_port(port: str) -> int:
    """Ouvre et configure un port série en 8N1, sans contrôle de flux."""
    descripteur = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attributs = termios.tcgetattr(descripteur)
        attributs[0] = 0  # Pas de traitement des octets reçus.
        attributs[1] = 0  # Pas de traitement des octets émis.
        attributs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
        attributs[3] = 0  # Mode non canonique, sans écho.
        attributs[4] = VITESSES[BAUDRATE]
        attributs[5] = VITESSES[BAUDRATE]
        attributs[6][termios.VMIN] = 0
        attributs[6][termios.VTIME] = 1
        termios.tcsetattr(descripteur, termios.TCSANOW, attributs)
        termios.tcflush(descripteur, termios.TCIFLUSH)
    except Exception:
        os.close(descripteur)
        raise
    return descripteur


def lire_et_journaliser(port: str) -> None:
    """Lit le port jusqu'à ce que l'utilisateur appuie sur Q."""
    chemin_log = nom_fichier_log(port)
    ancien_mode_terminal = termios.tcgetattr(sys.stdin.fileno())
    tampon = bytearray()
    descripteur = None

    try:
        descripteur = ouvrir_port(port)
        with chemin_log.open("w", encoding="utf-8") as fichier:
            tty.setcbreak(sys.stdin.fileno())
            print(
                f"\nLecture de {port} à {BAUDRATE} bauds."
                f"\nJournal : {chemin_log.resolve()}"
                "\nAppuyez sur Q pour quitter.\n",
                flush=True,
            )

            while True:
                lisibles, _, _ = select.select(
                    [sys.stdin.fileno(), descripteur], [], [], 0.1
                )

                if sys.stdin.fileno() in lisibles:
                    touche = os.read(sys.stdin.fileno(), 1)
                    if touche.lower() == b"q":
                        break

                if descripteur in lisibles:
                    morceau = os.read(descripteur, 4096)
                    tampon.extend(morceau)
                    while b"\n" in tampon:
                        ligne_brute, _, reste = tampon.partition(b"\n")
                        tampon = bytearray(reste)
                        enregistrer_ligne(fichier, ligne_brute)

            if tampon:
                enregistrer_ligne(fichier, bytes(tampon))

    except OSError as erreur:
        print(f"\nErreur d'accès au port ou au journal : {erreur}", file=sys.stderr)
        raise SystemExit(1)
    finally:
        if descripteur is not None:
            os.close(descripteur)
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, ancien_mode_terminal)

    print(f"\nPort fermé. Journal enregistré dans : {chemin_log.resolve()}")


def main() -> None:
    if not sys.stdin.isatty():
        print("Ce programme doit être exécuté dans un terminal.", file=sys.stderr)
        raise SystemExit(1)

    ports = lister_ports()
    if not ports:
        print("Aucun port série USB n'a été détecté.", file=sys.stderr)
        raise SystemExit(1)

    lire_et_journaliser(choisir_port(ports))


if __name__ == "__main__":
    main()
