"""Continuous serial logger that FLUSHES as it reads, so the log can be inspected live.

Usage: python serial_tail.py <port> <baud> <outfile>

Difference from serial_log.py: that one buffers everything in RAM and only writes
the file when the capture window ends — useless when we need to watch an event
(the user plugging something in) happen *now*.

Auto-reconnect: the Tab5 enumerates as a native USB CDC device, so the COM port
disappears and comes back every time it reboots (which it does on every panic).
Without this, one reboot silently ends the capture and we lose exactly the
crash output we were trying to read. Now we reconnect and keep logging, and we
stamp the log with markers so gaps are visible rather than silent.

DTR/RTS are explicitly held LOW right after opening: opening a port on Windows
asserts them by default; on the Tab5 (ESP32-P4 native USB-Serial/JTAG) they are
not wired to EN/IO0, but holding them low keeps us from emitting a reset edge.
"""
import sys
import time

import serial

port = sys.argv[1]
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
out = sys.argv[3] if len(sys.argv) > 3 else "serial_live.log"


def stamp(f, msg):
    f.write(f"\n### [{time.strftime('%H:%M:%S')}] {msg}\n")
    f.flush()


ser = None
reconnects = 0

with open(out, "w", encoding="utf-8", errors="replace", buffering=1) as f:
    stamp(f, f"live capture on {port} @ {baud}")
    while True:
        if ser is None:
            try:
                ser = serial.Serial(port, baud, timeout=0.2)
                try:
                    ser.dtr = False
                    ser.rts = False
                except Exception:
                    pass
                ser.reset_input_buffer()
                stamp(f, f"port opened ({reconnects} reconnect(s) so far)")
            except Exception as e:
                stamp(f, f"port busy/unavailable ({e.__class__.__name__}), retrying")
                time.sleep(1.0)
                continue

        try:
            data = ser.read(4096)
        except Exception as e:
            # Device rebooted (panic -> reboot -> USB re-enumeration) or cable glitch.
            stamp(f, f"link lost ({e.__class__.__name__}: {e}); waiting for {port}")
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            reconnects += 1
            time.sleep(0.6)
            continue

        if data:
            f.write(data.decode("utf-8", errors="replace"))
            f.flush()