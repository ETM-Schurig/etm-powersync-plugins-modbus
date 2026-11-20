#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only dump of the SunSpec storage block (model 124) of an inverter.

Answers one question: does this inverter have a battery physically installed?
See sunspec/DIVERGENCES.md D-6 — the "battery present" side of the presence
discriminator is still inferred and needs a reading from real hardware.

Standard library only: no pymodbus, no pysunspec2, nothing to install.
It only ever issues function code 3 (read holding registers). It never writes.

    python3 read-model124.py <ip-lan>
    python3 read-model124.py fronius.local --unit 1

Report back the InBatV, StorAval and ChaSt lines.
"""

import argparse
import socket
import struct
import sys

SUNS = (0x5375, 0x6E53)  # "SunS"

# Model 124, in order, from the SunSpec storage model definition.
# (name, offset, kind, scale-factor name or None)
POINTS = [
    ("WChaMax",      0,  "u16", "WChaMax_SF"),
    ("WChaGra",      1,  "u16", "WChaDisChaGra_SF"),
    ("WDisChaGra",   2,  "u16", "WChaDisChaGra_SF"),
    ("StorCtl_Mod",  3,  "u16", None),
    ("VAChaMax",     4,  "u16", "VAChaMax_SF"),
    ("MinRsvPct",    5,  "u16", "MinRsvPct_SF"),
    ("ChaState",     6,  "u16", "ChaState_SF"),
    ("StorAval",     7,  "u16", "StorAval_SF"),
    ("InBatV",       8,  "u16", "InBatV_SF"),
    ("ChaSt",        9,  "u16", None),
    ("OutWRte",     10,  "i16", "InOutWRte_SF"),
    ("InWRte",      11,  "i16", "InOutWRte_SF"),
    ("ChaGriSet",   15,  "u16", None),
]

SCALE_FACTORS = {
    "WChaMax_SF": 16, "WChaDisChaGra_SF": 17, "VAChaMax_SF": 18,
    "MinRsvPct_SF": 19, "ChaState_SF": 20, "StorAval_SF": 21,
    "InBatV_SF": 22, "InOutWRte_SF": 23,
}

CHA_ST = {1: "Off", 2: "Empty", 3: "Discharging", 4: "Charging",
          5: "Full", 6: "Holding", 7: "Testing"}

NOT_IMPLEMENTED_U16 = 0xFFFF
NOT_IMPLEMENTED_I16 = 0x8000


def read_holding(sock, unit, addr, count):
    req = struct.pack(">HHHBBHH", 1, 0, 6, unit, 3, addr, count)
    sock.sendall(req)
    hdr = b""
    while len(hdr) < 8:
        chunk = sock.recv(8 - len(hdr))
        if not chunk:
            raise IOError("connection closed while reading header")
        hdr += chunk
    _, _, length, _, fc = struct.unpack(">HHHBB", hdr)
    body = b""
    while len(body) < length - 2:
        chunk = sock.recv(length - 2 - len(body))
        if not chunk:
            raise IOError("connection closed while reading body")
        body += chunk
    if fc & 0x80:
        raise IOError("modbus exception %d reading %d registers at %d"
                      % (body[0], count, addr))
    return list(struct.unpack(">%dH" % (body[0] // 2), body[1:1 + body[0]]))


def find_base(sock, unit):
    for base in (40000, 50000, 0):
        try:
            if tuple(read_holding(sock, unit, base, 2)) == SUNS:
                return base
        except IOError:
            continue
    return None


def find_model(sock, unit, base, wanted):
    addr = base + 2
    chain = []
    while addr < base + 5000:
        mid, mlen = read_holding(sock, unit, addr, 2)
        if mid == 0xFFFF:
            return None, chain
        chain.append(mid)
        if mid == wanted:
            return addr + 2, chain
        addr += 2 + mlen
    return None, chain


def scaled(raw, kind, exponent):
    if kind == "i16" and raw > 0x7FFF:
        raw -= 0x10000
    if exponent is None:
        return float(raw)
    if exponent > 0x7FFF:
        exponent -= 0x10000
    return raw * (10.0 ** exponent)


def main():
    ap = argparse.ArgumentParser(description="Read-only dump of SunSpec model 124.")
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=502)
    ap.add_argument("--unit", type=int, default=1)
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=10)
    try:
        base = find_base(sock, args.unit)
        if base is None:
            print("No SunSpec 'SunS' marker at 40000, 50000 or 0 on unit %d." % args.unit)
            return 2
        print("SunSpec base register: %d (unit %d)" % (base, args.unit))

        data, chain = find_model(sock, args.unit, base, 124)
        print("Models present: %s" % ", ".join(str(m) for m in chain))
        if data is None:
            print("\nNo model 124 on this inverter: it publishes no storage block at all.")
            return 0

        regs = read_holding(sock, args.unit, data, 24)
    finally:
        sock.close()

    print("\nModel 124 raw block at %d:" % data)
    print("  " + " ".join("%04X" % r for r in regs))

    print("\n%-14s %10s  %s" % ("point", "value", "raw"))
    for name, off, kind, sf in POINTS:
        raw = regs[off]
        sentinel = NOT_IMPLEMENTED_I16 if kind == "i16" else NOT_IMPLEMENTED_U16
        if raw == sentinel:
            print("%-14s %10s  0x%04X" % (name, "n/impl", raw))
            continue
        value = scaled(raw, kind, regs[SCALE_FACTORS[sf]] if sf else None)
        extra = "  (%s)" % CHA_ST.get(raw, "?") if name == "ChaSt" else ""
        print("%-14s %10.3f  0x%04X%s" % (name, value, raw, extra))

    in_bat_v = scaled(regs[8], "u16", regs[SCALE_FACTORS["InBatV_SF"]])
    stor_aval = scaled(regs[7], "u16", regs[SCALE_FACTORS["StorAval_SF"]])
    print("\n--- what this decides -------------------------------------------")
    print("InBatV   = %.3f V" % in_bat_v)
    print("StorAval = %.3f Ah" % stor_aval)
    if in_bat_v > 0 or stor_aval > 0:
        print("=> reads as BATTERY PRESENT")
    else:
        print("=> reads as NO BATTERY")
    print("\nOn an inverter that really does have a battery, both being zero would")
    print("mean the discriminator is wrong and would hide a real battery. That is")
    print("exactly the case to report back.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
