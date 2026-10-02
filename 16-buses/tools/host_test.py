#!/usr/bin/env python3
"""
host_test.py - PC test program for the FX2LP <-> STM32 GPIF bridge.

Requires:  pip install pyusb libusb-package
Windows binds WinUSB automatically (MS OS descriptors in the FX2 firmware).

  python host_test.py status          FX2 bridge status (vendor request)
  python host_test.py reset           reset bridge FIFOs and the STM32
  python host_test.py info            STM32 version / clock / UID / counters
  python host_test.py led on|off      user LED on the STM32 side
  python host_test.py echo [N]        N echo frames with random data, verified
  python host_test.py stream [N]      N stream frames, verified, MB/s printed
"""
import os
import struct
import sys
import time

import usb.core
import usb.util

try:
    import libusb_package
    _find = libusb_package.find
except ImportError:
    _find = usb.core.find

VID, PID = 0x04B4, 0xF103
EP_OUT, EP_IN = 0x02, 0x86
FRAME = 512
HDR = 8
PAYLOAD_MAX = FRAME - HDR

VR_GET_STATUS = 0xB0
VR_RESET_BRIDGE = 0xB1

CMD_ECHO, CMD_SET_LED, CMD_GET_INFO = 0x01, 0x02, 0x03
CMD_STREAM_START, CMD_STREAM_STOP, CMD_STREAM_DATA = 0x04, 0x05, 0xC4


class Bridge:
    def __init__(self):
        self.dev = _find(idVendor=VID, idProduct=PID)
        if self.dev is None:
            sys.exit("device %04X:%04X not found" % (VID, PID))
        self.dev.set_configuration()
        usb.util.claim_interface(self.dev, 0)
        self.seq = 0

    # ---- control requests (handled by the FX2) ----------------------------
    def status(self):
        s = bytes(self.dev.ctrl_transfer(0xC0, VR_GET_STATUS, 0, 0, 8))
        return {
            "fx2_fw": "%d.%d" % (s[0], s[1]),
            "ACK(RDY0)": s[2] & 1,
            "DAV(RDY1)": (s[2] >> 1) & 1,
            "gpif_idle": (s[3] >> 7) & 1,
            "EP2 empty": (s[4] >> 1) & 1,
            "EP6 full": s[5] & 1,
            "high_speed": s[6],
            "configured": s[7],
        }

    def reset(self, reset_stm32=True):
        self.dev.ctrl_transfer(0x40, VR_RESET_BRIDGE, 1 if reset_stm32 else 0, 0, None)
        time.sleep(0.05)
        self._flush_in()

    def _flush_in(self):
        try:
            while True:
                self.dev.read(EP_IN, 64 * FRAME, timeout=50)
        except usb.core.USBTimeoutError:
            pass

    # ---- frames (go through the GPIF to the STM32) -------------------------
    def send(self, cmd, payload=b""):
        if len(payload) > PAYLOAD_MAX:
            raise ValueError("payload too long")
        self.seq = (self.seq + 1) & 0xFFFF
        frame = struct.pack("<BBHHBB", 0xA5, cmd, self.seq, len(payload), 0, 0)
        frame += payload + bytes(PAYLOAD_MAX - len(payload))
        self.dev.write(EP_OUT, frame, timeout=1000)
        return self.seq

    def recv(self, timeout=1000):
        data = bytes(self.dev.read(EP_IN, FRAME, timeout=timeout))
        if len(data) != FRAME or data[0] != 0x5A:
            raise IOError("bad frame (len %d, magic 0x%02X)" % (len(data), data[0] if data else 0))
        _, cmd, seq, length, status, _ = struct.unpack_from("<BBHHBB", data)
        return cmd, seq, status, data[HDR:HDR + length]

    def command(self, cmd, payload=b""):
        seq = self.send(cmd, payload)
        rcmd, rseq, status, body = self.recv()
        if rcmd != (cmd | 0x80) or rseq != seq:
            raise IOError("unexpected reply cmd=0x%02X seq=%d" % (rcmd, rseq))
        if status:
            raise IOError("STM32 returned status %d" % status)
        return body


def cmd_info(b):
    p = b.command(CMD_GET_INFO)
    ver, _, clk = struct.unpack_from("<HHI", p, 0)
    uid = p[8:20].hex().upper()
    names = ["rx_frames", "tx_frames", "timeouts", "resyncs", "underruns",
             "dropped_replies", "bad_frames", "uptime_ms"]
    vals = struct.unpack_from("<8I", p, 20)
    print("STM32 fw %d.%02d  SYSCLK %d MHz  UID %s" % (ver >> 8, ver & 0xFF, clk // 1000000, uid))
    for n, v in zip(names, vals):
        print("  %-16s %d" % (n, v))


def cmd_echo(b, n):
    t0 = time.perf_counter()
    for i in range(n):
        data = os.urandom(PAYLOAD_MAX)
        if b.command(CMD_ECHO, data) != data:
            sys.exit("echo mismatch at frame %d" % i)
    dt = time.perf_counter() - t0
    print("echo OK: %d frames, %.1f round trips/s" % (n, n / dt))


def cmd_stream(b, n):
    b.command(CMD_STREAM_START, struct.pack("<I", n))
    got = 0
    t0 = time.perf_counter()
    while got < n:
        want = min(n - got, 64) * FRAME
        data = bytes(b.dev.read(EP_IN, want, timeout=2000))
        for off in range(0, len(data), FRAME):
            f = data[off:off + FRAME]
            if f[0] != 0x5A or f[1] != CMD_STREAM_DATA:
                sys.exit("unexpected frame in stream at %d" % got)
            idx = struct.unpack_from("<I", f, HDR)[0]
            if idx != got:
                sys.exit("stream index %d, expected %d" % (idx, got))
            body = f[HDR + 4:]
            exp = bytes(((idx + i) & 0xFF) for i in range(4, PAYLOAD_MAX))
            if body != exp:
                sys.exit("stream data error in frame %d" % idx)
            got += 1
    dt = time.perf_counter() - t0
    print("stream OK: %d frames, %.2f MB/s" % (n, n * FRAME / dt / 1e6))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    b = Bridge()
    op = sys.argv[1]
    arg = sys.argv[2] if len(sys.argv) > 2 else None
    if op == "status":
        for k, v in b.status().items():
            print("  %-12s %s" % (k, v))
    elif op == "reset":
        b.reset(True)
        print("bridge and STM32 reset")
    elif op == "info":
        cmd_info(b)
    elif op == "led":
        b.command(CMD_SET_LED, bytes([1 if arg == "on" else 0]))
    elif op == "echo":
        cmd_echo(b, int(arg or 100))
    elif op == "stream":
        cmd_stream(b, int(arg or 2000))
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
