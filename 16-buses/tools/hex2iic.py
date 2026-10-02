#!/usr/bin/env python3
"""
hex2iic.py - convert a Keil C51 Intel-HEX file into an FX2LP "C2" boot EEPROM
image (.iic) for the 24LC128 on the board (I2C address 0x51).

C2 image layout (EZ-USB TRM, "EEPROM boot loading"):
    C2 VIDL VIDH PIDL PIDH DIDL DIDH CONFIG
    { LEN_H LEN_L ADDR_H ADDR_L data[LEN] } ...      LEN <= 1023
    80 01 E6 00 00                                   write CPUCS=0 -> run

usage: python hex2iic.py firmware.hex firmware.iic [--vid 0x04B4 --pid 0xF103]
"""
import argparse
import sys

MAX_RECORD = 1023


def read_ihex(path):
    mem = {}
    base = 0
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            if not line.startswith(":"):
                raise ValueError("line %d: not Intel-HEX" % lineno)
            raw = bytes.fromhex(line[1:])
            if (sum(raw) & 0xFF) != 0:
                raise ValueError("line %d: checksum error" % lineno)
            count, addr, rtype = raw[0], (raw[1] << 8) | raw[2], raw[3]
            data = raw[4:4 + count]
            if rtype == 0x00:
                for i, b in enumerate(data):
                    mem[base + addr + i] = b
            elif rtype == 0x01:
                break
            elif rtype == 0x02:
                base = ((data[0] << 8) | data[1]) << 4
            elif rtype == 0x04:
                base = ((data[0] << 8) | data[1]) << 16
            # 03/05 (start address) are ignored
    return mem


def contiguous_blocks(mem):
    addrs = sorted(mem)
    blocks = []
    start = prev = None
    for a in addrs:
        if start is None:
            start = prev = a
        elif a == prev + 1 and a - start < MAX_RECORD:
            prev = a
        else:
            blocks.append((start, prev))
            start = prev = a
    if start is not None:
        blocks.append((start, prev))
    return blocks


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("hexfile")
    ap.add_argument("iicfile")
    ap.add_argument("--vid", type=lambda s: int(s, 0), default=0x04B4)
    ap.add_argument("--pid", type=lambda s: int(s, 0), default=0xF103)
    ap.add_argument("--did", type=lambda s: int(s, 0), default=0x0000)
    ap.add_argument("--i2c100k", action="store_true",
                    help="boot at 100 kHz instead of 400 kHz")
    args = ap.parse_args()

    mem = read_ihex(args.hexfile)
    if not mem:
        sys.exit("no data in " + args.hexfile)

    for a in mem:
        if not (a < 0x4000 or 0xE000 <= a < 0xE200):
            sys.exit("address 0x%04X is outside FX2LP internal RAM" % a)

    out = bytearray([0xC2,
                     args.vid & 0xFF, args.vid >> 8,
                     args.pid & 0xFF, args.pid >> 8,
                     args.did & 0xFF, args.did >> 8,
                     0x00 if args.i2c100k else 0x40])
    for start, end in contiguous_blocks(mem):
        length = end - start + 1
        out += bytes([length >> 8, length & 0xFF, start >> 8, start & 0xFF])
        out += bytes(mem[a] for a in range(start, end + 1))
    out += bytes([0x80, 0x01, 0xE6, 0x00, 0x00])

    if len(out) > 16384:
        sys.exit("image is %d bytes - too large for a 24LC128" % len(out))

    with open(args.iicfile, "wb") as f:
        f.write(out)
    print("%s: %d bytes of code, image %d bytes" % (args.iicfile, len(mem), len(out)))


if __name__ == "__main__":
    main()
