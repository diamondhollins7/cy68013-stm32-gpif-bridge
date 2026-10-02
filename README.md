# CY7C68013A (FX2LP) ⇄ STM32F103 GPIF USB bridge

This repo holds a USB 2.0 High-Speed bridge. A **CY7C68013A-56PVXC** (EZ-USB FX2LP,
56-pin) is the GPIF master, and an **STM32F103VET6** is the slave. The repo
contains the schematic (PDF, netlist, BOM), Keil firmware for both chips, and
PC test tools.

| Folder | Version |
|---|---|
| [`8-bus/`](8-bus/README.md) | **v2, 8-bit GPIF bus.** FX2 firmware on the Cypress FX2LP framework; PC program in C++ using the Cypress driver (CyUSB3) and CyAPI; VID/PID 04B4:1004 |
| [`16-buses/`](16-buses/README.md) | v1, 16-bit GPIF bus. Stand-alone FX2 firmware with WinUSB auto-install; Python/pyusb test tool |

Each folder contains:

* `hardware/`: schematic PDF, netlist and BOM CSV files, and the Python generator (`gen_schematic.py`)
* `firmware/fx2/`: Keil C51 project for the CY7C68013A
* `firmware/stm32/`: Keil MDK-ARM project for the STM32F103VET6
* `docs/GPIF_interface.md`: signals, GPIF waveforms, frame protocol

Cypress/Infineon framework files (fw.c, EZUSB.LIB, ...) and CyAPI are **not**
included, because they are Cypress copyrighted. See
`8-bus/firmware/fx2/cypress_fw/README.txt` and `8-bus/host/cyapi/README.txt`
for where to get them.

Status: the sources have only been syntax-checked with MSVC. They have not been
built with Keil and have not been tested on hardware yet. See each README.
