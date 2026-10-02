# FX2LP (CY7C68013A-56) ⇄ STM32F103 — 8-bit GPIF USB bridge (v2)

This is the 8-bit version of the bridge. The 16-bit version is in `..\16-buses`.

The PC talks USB 2.0 High-Speed to a **CY7C68013A-56PVXC** through the **Cypress
driver (CyUSB3.sys) and CyAPI**. The FX2LP firmware is built on the **Cypress
FX2LP framework**. The FX2LP moves the data to and from an **STM32F103VET6** over
an **8-bit GPIF** bus.

```
PC (CyAPI) ──USB2.0 HS──> FX2LP EP2 OUT (AUTOOUT) ──GPIF 8-bit write──> STM32 PE[7:0]
PC (CyAPI) <──USB2.0 HS── FX2LP EP6 IN  (AUTOIN)  <──GPIF 8-bit read─── STM32 PE[7:0]
```

## Changes from the 16-bit version

| | 16-bit (`16-buses`) | 8-bit (this folder) |
|---|---|---|
| Data bus | FD[15:0] ⇄ PE[15:0], RN1..RN4 | FD[7:0] (PB0..7) ⇄ PE[7:0], RN1, RN2 |
| FX2 port D | FD[15:8] | unused GPIO, driven low |
| FX2 FIFOs | WORDWIDE = 1 | WORDWIDE = 0 on all four EPs |
| FX2 firmware | stand-alone | **Cypress FX2LP framework** (fw.c, EZUSB.LIB, USBJmpTb) |
| USB ID / driver | 04B4:F103, WinUSB (MS OS descriptors) | **04B4:1004, Cypress CyUSB3.sys** |
| PC program | Python + pyusb | **C++ with CyAPI** (`host/CyBridgeTest`) |
| Expected rate | ~2–4 MB/s | ~1–2 MB/s (one handshake per byte) |

The handshake lines (nWR, nRD, nSYNC, ACK, DAV), the 512-byte frame protocol and
the vendor requests 0xB0/0xB1 are unchanged. See `docs/GPIF_interface.md`.

## Contents

| Path | What |
|---|---|
| `hardware/FX2_STM32_GPIF8_Bridge_Schematic.pdf` | 5-sheet A3 schematic |
| `hardware/FX2_STM32_GPIF8_Bridge_Netlist.csv`, `..._BOM.csv` | netlist and bill of materials |
| `hardware/gen_schematic.py` | generator for the 3 files above (`pip install reportlab`) |
| `firmware/fx2/FX2_GPIF8_Bridge.uvproj` | Keil C51 project (Cypress framework) |
| `firmware/fx2/src/` | `bridge.c` (TD_/DR_ hooks, ISRs), `gpif.c`, `dscr.a51`, `bridge.h` |
| `firmware/fx2/cypress_fw/` | **you copy the 6 Cypress framework files here** (see README.txt) |
| `firmware/stm32/STM32_GPIF_Slave.uvprojx` | Keil MDK-ARM project, STM32 8-bit GPIF slave |
| `host/CyBridgeTest/` | Visual Studio 2022 C++ console program using CyAPI |
| `host/cyapi/` | **you copy CyAPI.h / CyAPI.lib here** (see README.txt) |
| `tools/hex2iic.py` | `.hex` → C2 boot EEPROM `.iic` |

## 1. FX2LP firmware (Keil C51 + Cypress framework)

1. Copy these files from your CY3684 FX2LP DVK installation into
   `firmware/fx2/cypress_fw/`: `fw.c`, `fx2.h`, `fx2regs.h`, `syncdly.h`,
   `EZUSB.LIB`, `USBJmpTb.OBJ`. The usual locations are listed in
   `cypress_fw/README.txt`.
2. Open `firmware/fx2/FX2_GPIF8_Bridge.uvproj` in µVision. If it asks for a
   device, select Cypress CY7C68013A.
3. Build. This produces `Objects/fx2_gpif8_bridge.hex`, and the after-build step
   also creates `Objects/fx2_gpif8_bridge.iic`. The framework plus this code is
   larger than the 2 KB C51 evaluation limit, so a licensed C51 is required (the
   Keil version shipped with the Cypress kit also works).

How the code plugs into the framework:

* `fw.c` owns `main()`, enumeration and the standard requests, using the
  descriptors in `src/dscr.a51`.
* `TD_Init()` sets up the 48 MHz clock, IFCONFIG for GPIF master mode, EP2/EP6,
  the 8-bit AUTO FIFOs and the GPIF waveforms. It then resets the STM32 and
  pulses nSYNC.
* `TD_Poll()` starts the GPIF transfers: STM32→EP6 when DAV = 1, and EP2→STM32
  when an OUT packet is waiting.
* `DR_VendorCmnd()` handles 0xB0 (status) and 0xB1 (bridge reset).
* All USB/GPIF ISRs required by `USBJmpTb.OBJ` are defined in `bridge.c`. If your
  framework version reports an unresolved `ISR_xxx`, add an empty
  `void ISR_xxx(void) interrupt 0 { }`.

### Loading / programming the FX2LP

With JP1 open (EEPROM disconnected), the FX2LP enumerates as `04B4:8613`. The
Cypress driver binds to that ID, so Cypress **USB Control Center** can:

* *Program → FX2 → RAM* with `fx2_gpif8_bridge.hex` (quick test). The device
  re-enumerates as **04B4:1004 "FX2-STM32 8-bit Bridge"**.
* *Program → FX2 → 64KB EEPROM* with `fx2_gpif8_bridge.iic`. Close JP1 after
  enumeration and before programming. After a power cycle the bridge boots by
  itself.

## 2. Cypress driver

Install `cyusb3.inf` from the EZ-USB FX3 SDK (`driver\bin\<OS>\x64`) for both
`04B4:8613` (blank FX2LP) and `04B4:1004` (this firmware). 04B4:1004 is a Cypress
sample ID. If your `cyusb3.inf` does not list `VID_04B4&PID_1004`, see
`host/cyapi/README.txt`. For a product, use your own VID/PID in `dscr.a51` and in
`main.cpp`.

## 3. PC program (C++ / CyAPI)

1. Copy `CyAPI.h` (and the other headers next to it) plus `CyAPI.lib` (x64 and
   x86) from the FX3 SDK `library\cpp` folder into `host/cyapi/`. See
   `host/cyapi/README.txt`.
2. Open `host/CyBridgeTest/CyBridgeTest.vcxproj` in Visual Studio 2022 and build
   x64 Release. The project links `CyAPI.lib`, `setupapi.lib` and
   `legacy_stdio_definitions.lib` and uses the static CRT (/MT). If you get an
   LNK2038 "RuntimeLibrary mismatch" error with your CyAPI.lib, switch C/C++ →
   Code Generation → Runtime Library to /MD.
3. Run it:

```
CyBridgeTest list            devices on the Cypress driver
CyBridgeTest status          FX2: firmware, ACK/DAV, GPIF idle, FIFO flags, speed
CyBridgeTest info            STM32 version, clock, UID, counters (through GPIF)
CyBridgeTest echo 1000       1000 x 504-byte random echo, verified
CyBridgeTest stream 20000    STM32 -> PC stream, verified, prints MB/s
CyBridgeTest led on|off
CyBridgeTest reset           flush bridge + reset STM32
```

## 4. STM32 firmware (Keil MDK-ARM)

The setup is the same as the 16-bit version: install Keil::STM32F1xx_DFP, open
`firmware/stm32/STM32_GPIF_Slave.uvprojx`, select ST-Link, then build and
download. The UART (115200 8N1) prints `STM32F103 GPIF8 slave v2.00`. Only
PE0..PE7 are used for data, so PE8..PE15 are free.

## Bring-up checklist

1. Check +5V and +3V3 and the power LED D1.
2. Flash the STM32. Check the UART banner and the D4 heartbeat.
3. Load the FX2 `.hex` into RAM. Run `CyBridgeTest list` and confirm it shows
   04B4:1004, high-speed.
4. Run `CyBridgeTest status`. Expect configured 1, GPIF idle 1, DAV 0.
5. With a logic analyser on J4, run `echo 1`. Each byte should show nWR↓ ACK↑ nWR↑ ACK↓.
6. Run `echo 1000` and `stream 20000`, then program the EEPROM.

## Verification status

* These were **not** built with Keil, the Cypress framework or the real CyAPI here,
  because none of them is installed on this PC. Instead, each source was
  compiled with MSVC against small stand-in headers that declare the framework
  and CyAPI symbols it uses: the FX2 code, the STM32 code (8-bit) and
  `CyBridgeTest`. All three compile without errors.
* The framework hook names and macros (`TD_*`, `DR_*`, `ISR_*`, `EZUSB_*`,
  `CONFIGDSCR`, `pConfigDscr`) follow the Cypress CY3684 examples (bulkloop).
  Check them against your framework version.
* Nothing has been run on hardware yet. The GPIF waveform table is the same,
  decoded and checked, as in the 16-bit version.
