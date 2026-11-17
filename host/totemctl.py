#!/usr/bin/env python3
"""Send protocol commands to the totem: totemctl.py [--port P] LASER EYE PULSE 50

Special words: `monitor` (print everything the cat sends), `selftest`.
"""

import argparse
import glob
import os
import sys
import time

import serial

PREFIXES = ("OK", "ERR", "PONG")


def send(ser, line, wait=1.0):
    ser.write((line + "\n").encode("ascii"))
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        reply = ser.readline().decode("ascii", "replace").strip()
        if reply.startswith(PREFIXES):  # ignore debug lines
            return reply
    return None


def selftest(ser):
    steps = [
        ("PING", "PONG"), ("ID", "OK TOTEM"), ("STATUS", "OK arm="),
        ("LED LEVEL 128", "OK"), ("LED OFF", "OK"),
        ("LASER EYE PULSE 50", "OK"), ("LASER HAND PULSE 50", "OK"),
        ("LASER COLLAR1 PULSE 50", "OK"), ("COLLAR AUTO", "OK"),
        ("WAVE TINY", "OK"), ("SERVO 90", "OK"),
        ("BEAT 500", "OK"), ("BEAT OFF", "OK"),
        ("LASER EYE BOGUS", "ERR args"), ("SERVO 999", "ERR range"),
        ("TIMEOUT 1000", "OK"),
    ]
    ok = True
    for cmd, want in steps:
        reply = send(ser, cmd)
        good = bool(reply) and reply.startswith(want)
        ok &= good
        print("PASS" if good else "FAIL", cmd, "->", reply)
    time.sleep(1.6)  # host silent: failsafe must start the party
    reply = send(ser, "STATUS")
    good = bool(reply) and "party=1" in reply
    ok &= good
    print("PASS" if good else "FAIL", "party mode ->", reply)
    send(ser, "TIMEOUT 5000")
    send(ser, "OFF")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default=os.environ.get("TOTEM_PORT"))
    ap.add_argument("command", nargs="+")
    args = ap.parse_args()

    port = args.port or next(iter(sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))), None)
    if not port:
        sys.exit("no serial port found; use --port or TOTEM_PORT")

    with serial.Serial(port, 115200, timeout=0.2) as ser:
        time.sleep(2)  # opening the port resets a Nano; wait for the bootloader
        ser.reset_input_buffer()
        word = args.command[0].lower()
        if word == "selftest":
            return 0 if selftest(ser) else 1
        if word == "monitor":
            try:
                while True:
                    print(ser.readline().decode("ascii", "replace"), end="", flush=True)
            except KeyboardInterrupt:
                return 0
        reply = send(ser, " ".join(args.command))
        print(reply)
        return 0 if reply and not reply.startswith("ERR") else 1


if __name__ == "__main__":
    sys.exit(main())
