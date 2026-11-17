# Totem UART protocol

USB serial of the Arduino Nano, **115200 8N1**. The same port is used for
flashing, so close `pio device monitor` before talking to the cat.

One ASCII command per line (`\n` or `\r\n`), case-insensitive, max 63 chars.
Replies start with `OK`, `ERR` or `PONG`; ignore any other line (debug output).

| Command | Reply |
|---|---|
| `PING` | `PONG` |
| `ID` | `OK TOTEM 1` |
| `STATUS` | `OK arm=<deg> move=<0/1> collar=<auto/manual> beat=<ms> party=<0/1>` |
| `SERVO <0..180>` / `SERVO STOP` | `OK` |
| `WAVE TINY` / `WAVE FULL` / `WAVE STOP` | `OK` (one request is queued while the arm moves) |
| `LASER <target> ON\|OFF` | `OK` |
| `LASER <target> PULSE [ms]` (default 50 for `EYE`, else 100) | `OK` |
| `COLLAR AUTO` (resume built-in chase) | `OK` |
| `LED ON\|OFF` | `OK` |
| `LED LEVEL <0..255>` | `OK` |
| `LED BLINK <on_ms> <off_ms>` | `OK` |
| `LED BREATHE <period_ms>` | `OK` |
| `BEAT <20..60000>` / `BEAT OFF` (arm + eye lasers blink on the beat) | `OK` |
| `TIMEOUT <ms>` (failsafe, `0` disables, default 5000) | `OK` |
| `OFF` | `OK` |

Targets: `EYE`, `HAND`, `COLLAR1`, `COLLAR2`, `COLLAR3`, `COLLAR` (all three),
`ALL`. Any collar target stops the built-in collar chase until `COLLAR AUTO`.

Errors: `ERR args`, `ERR range`, `ERR unknown`, `ERR line` (line too long).

## Failsafe: party mode

If the host has sent a command and then stays silent for `TIMEOUT` ms, the cat
starts **party mode**: arm + eye lasers blink at the last beat (500 ms default)
and the collar chase resumes. `PING`, `ID`, `STATUS` and `TIMEOUT` do not end
it; any other command takes over. `OFF` turns everything off and disarms the
failsafe until the next command. The lasers keep blinking if the PC dies, only
`OFF` or power-off stops them.

## Hardware

- LED chain MOSFET gate is on `D3` (PWM), so `LED LEVEL` / `BREATHE` dim.
- The button inputs keep working in parallel.

## CLI

```
python3 -m pip install -r host/requirements.txt
./host/totemctl.py --port /dev/ttyUSB0 PING
./host/totemctl.py LASER EYE PULSE 50     # port: $TOTEM_PORT or first ttyUSB/ttyACM
./host/totemctl.py selftest
./host/totemctl.py monitor
```

`Permission denied`: add yourself to `dialout` and log in again.
