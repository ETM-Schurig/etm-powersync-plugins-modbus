#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Which word order does a SunSpec inverter use for its float32 values?

The SunSpec specification says big endian, most significant word first. But
libnymea-sunspec decodes `ByteOrderLittleEndian` by default, that is
`registers.at(1)` THEN `registers.at(0)` — least significant word first
(sunspecdatapoint.cpp, convertToUInt32). The plugin's Fronius vendor keeps this
default. If the device sends big endian, the plugin reads denormals (~1e-41)
and no SIGN correction will change anything about it.

This script settles the question by measurement, on two points whose expected
value is known in advance and unambiguous:
  - Hz      : ~50 Hz
  - PhVphA  : ~230..240 V
One order gives these values, the other gives denormals. There is no possible
doubt between the two.

READ ONLY: this script sends ONLY Modbus function 3 (read holding
registers). No write, no write function code is implemented.

    python3 read-float-order.py <ip-lan>
    python3 read-float-order.py <ip-lan> --inverter-unit 1 --meter-unit 200

Standard library only: nothing to install.
"""

import argparse
import socket
import struct
import sys

SUNS = (0x5375, 0x6E53)

# Offsets counted from the model's ID register (the data start at +2).
# Each float32 point takes TWO registers, hence the step of 2.
INV_F = {"W": 22, "Hz": 24, "PhVphA": 16, "PhVphB": 18, "PhVphC": 20, "DCW": 38}
MET_F = {"W": 28, "WphA": 30, "WphB": 32, "WphC": 34,
         "Hz": 26, "PhV": 10, "PhVphA": 12, "PhVphB": 14, "PhVphC": 16}

INVERTER_FLOAT = (111, 112, 113)
METER_FLOAT = (211, 212, 213, 214)
INVERTER_INT = (101, 102, 103)
METER_INT = (201, 202, 203, 204)


def read_holding(sock, unit, addr, count):
    """Function 3, and nothing else."""
    sock.sendall(struct.pack(">HHHBBHH", 1, 0, 6, unit, 3, addr, count))
    hdr = b""
    while len(hdr) < 8:
        chunk = sock.recv(8 - len(hdr))
        if not chunk:
            raise IOError("connexion fermée (en-tête)")
        hdr += chunk
    _, _, length, _, fc = struct.unpack(">HHHBB", hdr)
    body = b""
    while len(body) < length - 2:
        chunk = sock.recv(length - 2 - len(body))
        if not chunk:
            raise IOError("connexion fermée (corps)")
        body += chunk
    if fc & 0x80:
        raise IOError("exception modbus %d en lisant %d registres à %d"
                      % (body[0], count, addr))
    return list(struct.unpack(">%dH" % (body[0] // 2), body[1:1 + body[0]]))


def decode(r0, r1, low_word_first):
    hi, lo = (r1, r0) if low_word_first else (r0, r1)
    return struct.unpack(">f", struct.pack(">HH", hi, lo))[0]


def find_base(sock, unit):
    for base in (40000, 50000, 0):
        try:
            if tuple(read_holding(sock, unit, base, 2)) == SUNS:
                return base
        except IOError:
            continue
    return None


def walk(sock, unit, base):
    """(model_id, address of the ID register, length) for the whole chain."""
    out, addr = [], base + 2
    while addr < base + 5000:
        mid, mlen = read_holding(sock, unit, addr, 2)
        if mid == 0xFFFF:
            return out
        out.append((mid, addr, mlen))
        addr += 2 + mlen
    return out


def verdict(hz_low, hz_big):
    """Which order gives a plausible grid frequency?"""
    def plausible(v):
        return 45.0 <= v <= 65.0
    if plausible(hz_low) and not plausible(hz_big):
        return "MOT DE POIDS FAIBLE D'ABORD (ce que le plugin lit déjà)"
    if plausible(hz_big) and not plausible(hz_low):
        return ("BIG ENDIAN, conforme à la spec SunSpec — un plugin qui garde le "
                "défaut de libnymea-sunspec lira des dénormaux")
    if hz_low == 0.0 and hz_big == 0.0:
        return "INDÉCIDABLE : registres à zéro, aucun ordre ne se distingue"
    return "INDÉCIDABLE : aucune des deux lectures ne donne 45..65 Hz"


def probe(host, port, unit, wanted_float, wanted_int, table, points, role):
    print("=" * 74)
    print("%s — unité %d" % (role, unit))
    print("=" * 74)
    sock = socket.create_connection((host, port), timeout=10)
    try:
        base = find_base(sock, unit)
        if base is None:
            print("  aucun marqueur « SunS » : rien à cette unité.\n")
            return
        chain = walk(sock, unit, base)
        print("  base %d · chaîne : %s" % (
            base, ", ".join("%d@%d(L=%d)" % (m, a, l) for m, a, l in chain)))

        found = [c for c in chain if c[0] in wanted_float]
        if not found:
            ints = [c for c in chain if c[0] in wanted_int]
            if ints:
                print("  modèle %d : ENTIER+SF, pas de float32 — la question de "
                      "l'ordre des mots ne se pose pas ici.\n" % ints[0][0])
            else:
                print("  aucun modèle attendu dans cette chaîne.\n")
            return

        mid, id_addr, mlen = found[0]
        print("  modèle float %d, registre ID %d, longueur %d\n" % (mid, id_addr, mlen))
        print("  %-8s %-9s %-14s %14s %16s  %s" % (
            "point", "registre", "brut (hex)", "mot faible 1er", "big endian", "lecture"))
        print("  " + "-" * 86)
        hz = None
        for name in points:
            off = table[name]
            addr = id_addr + off
            r0, r1 = read_holding(sock, unit, addr, 2)
            low, big = decode(r0, r1, True), decode(r0, r1, False)
            if name == "Hz":
                hz = (low, big)
            note = "nul — n'apprend rien" if (r0 == 0 and r1 == 0) else ""
            print("  %-8s %-9d %-14s %14.4f %16.4e  %s" % (
                name, addr, "%04X %04X" % (r0, r1), low, big, note))
        print()
        if hz:
            print("  >>> %s" % verdict(*hz))
        print()
    finally:
        sock.close()


def main():
    ap = argparse.ArgumentParser(
        description="Ordre des mots des float32 SunSpec — LECTURE SEULE.")
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=502)
    ap.add_argument("--inverter-unit", type=int, default=1)
    ap.add_argument("--meter-unit", type=int, default=200)
    args = ap.parse_args()

    # The METER first: it is the one that decides. It measures the grid
    # permanently, so its registers carry values even with the inverter on standby.
    # An idle inverter serves a block of zeros, and zero decodes to 0.0 in BOTH
    # orders: that teaches nothing.
    probe(args.host, args.port, args.meter_unit, METER_FLOAT, METER_INT,
          MET_F, ("Hz", "PhVphA", "PhVphB", "PhVphC", "W", "WphA"),
          "COMPTEUR — mesure décisive")
    probe(args.host, args.port, args.inverter_unit, INVERTER_FLOAT, INVERTER_INT,
          INV_F, ("Hz", "PhVphA", "PhVphB", "PhVphC", "W"),
          "ONDULEUR — confirmation, nul si en veille")

    print("Lecture : la colonne qui donne ~50 Hz et ~230..240 V est l'ordre que")
    print("l'appareil émet. L'autre affiche des dénormaux (~1e-41) : c'est la")
    print("signature d'une mauvaise interprétation, pas une vraie mesure.")
    print()
    print("Si « big endian » est la bonne colonne, tout lecteur qui garde le défaut")
    print("de libnymea-sunspec (mot faible d'abord) lira ces modèles de travers.")
    print("Le vendor Fronius du plugin force big endian depuis 1.15.2+etm9.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
