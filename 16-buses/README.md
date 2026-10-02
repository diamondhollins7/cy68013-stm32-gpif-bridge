# FX2LP (CY7C68013A-56) ⇄ STM32F103 GPIF USB bridge

The PC talks USB 2.0 High-Speed to a **CY7C68013A-56PVXC** (EZ-USB FX2LP, 56-pin
SSOP). The FX2LP moves the data to and from an **STM32F103VET6** over a 16-bit
**GPIF** parallel bus.

```
PC ──USB2.0 HS──> FX2LP EP2 OUT (AUTOOUT) ──GPIF write──> STM32  (PE[15:0])
PC <──USB2.0 HS── FX2LP EP6 IN  (AUTOIN)  <──GPIF read─── STM32
```

## Contents

| Path | What |
|---|---|
| `hardware/FX2_STM32_GPIF_Bridge_Schematic.pdf` | 5-sheet A3 schematic: overview, USB/power, FX2LP, STM32, GPIF interconnect + timing |
| `hardware/FX2_STM32_GPIF_Bridge_Netlist.csv` | full netlist (net, ref, pin) for PCB capture / checking |
| `hardware/FX2_STM32_GPIF_Bridge_BOM.csv` | bill of materials |
| `hardware/gen_schematic.py`, `schematic_lib.py` | generator for the 3 files above (`pip install reportlab`) |
| `firmware/fx2/FX2_GPIF_Bridge.uvproj` | **Keil C51** project for the CY7C68013A |
| `firmware/stm32/STM32_GPIF_Slave.uvprojx` | **Keil MDK-ARM** project for the STM32F103VET6 |
| `tools/hex2iic.py` | converts the FX2 `.hex` to a C2 boot-EEPROM `.iic` image |
| `tools/host_test.py` | PC test program: status, echo, LED, info, throughput stream |
| `docs/GPIF_interface.md` | signals, waveform state tables, frame protocol |

## Hardware summary

* **U1 CY7C68013A-56PVXC**, 24 MHz crystal, 24LC128 boot EEPROM (I²C 0x51, C2 load).
  JP1 disconnects the EEPROM so the chip can be recovered.
* **U5 STM32F103VET6**, 8 MHz HSE → 72 MHz, SWD (J2), UART1 debug (J3), BOOT0 jumper (JP2).
* GPIF bus: FD[15:0] ⇄ PE[15:0] through 33 Ω arrays. Handshake lines:
  CTL0 = nWR → PC0, CTL1 = nRD → PC1, CTL2 = nSYNC → PC4, RDY0 ← ACK PC2,
  RDY1 ← DAV PC3. FX2 PA0 can reset the STM32.
* Bus powered: USB Micro-B, USBLC6-2 ESD protection, 500 mA PTC fuse, AMS1117-3.3.
  Separate ferrite-filtered analog supplies feed FX2 AVCC and STM32 VDDA.

Pin numbers come from the CY7C68013A datasheet (38-08032 Rev AD, table 11, 56-SSOP
column) and the STM32F103xE pin table (LQFP100). The generator checks that all
56 + 100 IC pins are placed and that every net has at least two connections.

> The schematic is delivered as a PDF plus a netlist. It is not a native Altium
> `.SchDoc`. To lay out the PCB, recreate it in Altium (or KiCad) from the PDF and
> `Netlist.csv`, then compare your netlist against the CSV.

## 1. FX2LP firmware (Keil C51)

1. Open `firmware/fx2/FX2_GPIF_Bridge.uvproj` in µVision (C51 installed).
   If µVision asks for a device, select **Cypress → CY7C68013A**. Any plain
   8051 entry also works, because all registers are defined in `src/fx2regs.h`.
2. Project settings already set: memory model *Small*, Intel HEX output.
3. **Build** (F7). This produces `Objects/fx2_gpif_bridge.hex`. The after-build
   step runs `python ..\..\tools\hex2iic.py` to create `Objects/fx2_gpif_bridge.iic`.
   If Python is not on your PATH, run that command manually.
4. The C51 evaluation version is limited to 2 KB of code. This firmware is close
   to that limit, so a licensed C51 (PK51) may be needed.

No Cypress framework files (`fw.c`, `EZUSB.LIB`, ...) are needed. USB enumeration
is handled by polling (no interrupt vectors). The data path never touches the 8051.

### Loading / programming the FX2LP

* **Quick test (RAM):** open JP1 and plug in USB. The chip enumerates as
  `04B4:8613`. Load `fx2_gpif_bridge.hex` with Cypress *USB Control Center*
  (Program → FX2 → RAM) or with open-source `fx2tool load` (from `pip install libfx2`,
  after binding WinUSB to `04B4:8613` with Zadig). The device then re-enumerates
  as **`04B4:F103` "FX2-STM32 Bridge"**.
* **Permanent (EEPROM):** boot with JP1 open, close JP1 after enumeration, then
  in USB Control Center choose *Program → FX2 → 64KB EEPROM* and select
  `fx2_gpif_bridge.iic`. Power-cycle with JP1 closed.

Windows binds **WinUSB automatically** through the Microsoft OS 1.0 descriptors,
so no INF or driver is needed. Change `USB_VID/USB_PID` in `src/bridge.h` (and
`--vid/--pid` of hex2iic) to your own IDs before shipping a product.

## 2. STM32 firmware (Keil MDK-ARM)

1. Install the pack **Keil::STM32F1xx_DFP** (Pack Installer). It supplies the
   device entry and flash algorithm.
2. Open `firmware/stm32/STM32_GPIF_Slave.uvprojx`. The project uses Arm Compiler 6
   and MicroLIB. It does not depend on HAL, StdPeriph or RTE components: the
   startup file and the register header are in `src/`.
3. *Options → Debug*: select **ST-Link Debugger** (SWD). *Utilities*: "Use Debug Driver".
4. Build (F7) and Download (F8). The UART at 115200 8N1 prints
   `STM32F103 GPIF slave v1.00`, and LED D4 blinks at 1 Hz.

If you prefer, the project also builds with Arm Compiler 5. In that case
`__NOP()` maps to `__nop()`.

## 3. PC test

```
pip install pyusb libusb-package
python tools/host_test.py status        # FX2: speed, configured, ACK/DAV, FIFO flags
python tools/host_test.py info          # STM32 version, clock, UID, counters (through GPIF)
python tools/host_test.py echo 1000     # 1000 x 504-byte random echo, verified
python tools/host_test.py stream 20000  # STM32 -> PC stream, verified, prints MB/s
python tools/host_test.py led on
python tools/host_test.py reset         # vendor request 0xB1: flush + reset STM32
```

## Bring-up checklist

1. With no ICs fitted or no firmware loaded, check +5V and +3V3 (TP1/TP2) and the
   power LED D1.
2. Flash the STM32 through J2. Check the UART banner, the D4 heartbeat, and that
   PC2/PC3 (ACK/DAV) are low.
3. Load the FX2 firmware into RAM, as described above. Check that the device is
   `04B4:F103` and that `status` reports `high_speed 1`.
4. On a logic analyser at J4, run `echo 1`. For each word you should see nWR fall,
   ACK rise, nWR rise, then ACK fall.
5. Run `echo 1000`, then `stream 20000`.
6. Program the EEPROM and test again after a power cycle.

## Verification status

* FX2 and STM32 sources pass a host-compiler syntax check (MSVC `/W4`, with a
  compatibility shim for the C51 keywords). They were **not** built with Keil
  here, and they have **not been run on hardware**.
* The FX2 register addresses and bit layouts used were cross-checked against the
  datasheet register summary. The GPIF waveform encoding follows the EZ-USB TRM
  descriptor format (LENGTH/BRANCH, OPCODE, OUTPUT, LOGFUNC). Verify it on the
  first board with a logic analyser (step 4).
* Throughput has not been measured. The estimate is 2–4 MB/s, limited by the
  STM32's GPIO polling.
