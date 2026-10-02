#!/usr/bin/env python3
"""
gen_schematic.py - generates the schematic PDF, netlist and BOM of the
CY7C68013A (56-SSOP) + STM32F103VET6 8-bit GPIF bridge board.

    pip install reportlab
    python gen_schematic.py

Outputs (same folder):
    FX2_STM32_GPIF_Bridge_Schematic.pdf
    FX2_STM32_GPIF_Bridge_Netlist.csv   (ref, pin, pin name, net)
    FX2_STM32_GPIF_Bridge_BOM.csv

Pin numbers: CY7C68013A 56-SSOP from datasheet 38-08032 Rev. AD table 11,
STM32F103VET6 LQFP100 from DS5792 table 5.
"""
import csv
import os
import sys
from collections import defaultdict

from reportlab.pdfgen import canvas as rl_canvas
from reportlab.lib.units import mm

import schematic_lib as S
from schematic_lib import Netlist, Sheet, PAGE_W, PAGE_H

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, "FX2_STM32_GPIF8_Bridge")
PROJECT = "FX2LP - STM32F103 GPIF USB Bridge (8-bit)"
REV = "2.0"
DATE = "2026-10-02"
NSHEETS = 5

S.POWER_NETS.update({"+5V", "+3V3", "+3V3_FXA", "+3V3_STA", "VBUS_CONN"})

FP_0603 = "0603"
FP_0805 = "0805"

nl = Netlist()


def new_sheet(c, n, title):
    sh = Sheet(c, nl, n, NSHEETS, title, PROJECT, REV, DATE)
    sh.frame()
    return sh


# =============================================================================
# Sheet 1 - overview / block diagram / interface table
# =============================================================================
def sheet_overview(c):
    sh = new_sheet(c, 1, "Overview & GPIF interface")
    sh.text(15, 18, "FX2LP (CY7C68013A-56PVXC) <-> STM32F103VET6 : USB 2.0 High-Speed bridge over an 8-bit GPIF bus",
            size=3.6, bold=True)

    def box(x, y, w, h, title, lines=(), fill=S.C_FILL):
        sh.rect(x, y, w, h, color=S.C_LINE, width=0.4, fill=fill)
        sh.text(x + w / 2, y + 6, title, size=3.0, bold=True, anchor="c")
        for i, ln in enumerate(lines):
            sh.text(x + w / 2, y + 12 + i * 4.2, ln, size=2.2, anchor="c")

    def arrow(x1, y1, x2, y2, text=None, both=False, tx=None, ty=None):
        sh.line(x1, y1, x2, y2, S.C_WIRE, 0.45)
        import math
        a = math.atan2(y2 - y1, x2 - x1)

        def head(x, y, ang):
            p1 = (x - 2.6 * math.cos(ang - 0.4), y - 2.6 * math.sin(ang - 0.4))
            p2 = (x - 2.6 * math.cos(ang + 0.4), y - 2.6 * math.sin(ang + 0.4))
            sh.poly([(x, y), p1, p2], color=S.C_WIRE, width=0.2, fill=S.C_WIRE)
        head(x2, y2, a)
        if both:
            head(x1, y1, a + math.pi)
        if text:
            sh.text(tx if tx is not None else (x1 + x2) / 2,
                    ty if ty is not None else (y1 + y2) / 2 - 1.5,
                    text, size=2.1, anchor="c")

    box(15, 40, 42, 36, "PC / USB host", ["Cypress driver CyUSB3.sys", "CyAPI (C++)", "host/CyBridgeTest"],
        fill=None)
    box(80, 44, 30, 28, "J1", ["USB Micro-B", "U4 USBLC6-2 ESD"])
    box(135, 32, 70, 70, "U1  CY7C68013A", ["EZ-USB FX2LP, 56-SSOP", "8051 @ 48 MHz", "GPIF master, IFCLK 48 MHz",
                                           "EP2 OUT  bulk 512 B x2", "EP6 IN   bulk 512 B x2",
                                           "AUTOOUT / AUTOIN, 8-bit FIFOs", "Cypress FX2LP framework"])
    box(285, 32, 75, 70, "U5  STM32F103VET6", ["Cortex-M3 @ 72 MHz, LQFP100", "GPIF slave (GPIO polled)",
                                               "PE[7:0] data bus", "PC0..PC4 handshake",
                                               "8 x 512 B TX frame queue"])
    box(135, 122, 70, 26, "U2  24LC128", ["I2C boot EEPROM (addr 0x51)", "C2 image, JP1 = disconnect"])
    box(15, 122, 90, 40, "Power", ["USB VBUS -> F1 500 mA -> FB1 -> +5V", "U3 AMS1117-3.3 -> +3V3",
                                   "FB2 -> +3V3_FXA (FX2 AVCC)", "FB3 -> +3V3_STA (STM32 VDDA)"])
    box(285, 122, 75, 40, "STM32 peripherals", ["J2 SWD (ST-Link)", "J3 UART1 debug 115200 8N1",
                                                "D4 heartbeat, D5 user LED", "Y2 8 MHz, SW2 reset, JP2 BOOT0"])

    arrow(57, 58, 80, 58, "USB", both=True)
    arrow(110, 58, 135, 58, "D+/D- (HS)", both=True)
    sh.text(245, 37, "33R series resistors RN1, RN2", size=2.0, anchor="c", color=S.C_GREY)
    arrow(205, 44, 285, 44, "FD[7:0]  <->  PE[7:0]   8-bit data", both=True)
    arrow(205, 54, 285, 54, "CTL0  nWR  ->  PC0")
    arrow(205, 62, 285, 62, "CTL1  nRD  ->  PC1")
    arrow(205, 70, 285, 70, "CTL2  nSYNC  ->  PC4")
    arrow(285, 78, 205, 78, "RDY0  <-  ACK  PC2")
    arrow(285, 86, 205, 86, "RDY1  <-  DAV  PC3")
    arrow(205, 96, 285, 96, "PA0  ->  NRST (open-drain, R16 100R)")
    arrow(170, 102, 170, 122, None, both=True)
    sh.text(172, 113, "I2C SCL/SDA", size=2.1)

    # signal table
    rows = [
        ("Signal", "FX2 pin (56-SSOP)", "Net", "STM32 pin (LQFP100)", "Dir", "Function"),
        ("FD0..FD7", "PB0..PB7 : 25..32", "FD0..7 / GD0..7", "PE0..PE7 : 97,98,1,2,3,4,5,38", "<->", "8-bit data bus (USB byte stream)"),
        ("nWR", "CTL0 : 36", "GPIF_NWR", "PC0 : 15", "->", "FX2 write strobe, low = data valid"),
        ("nRD", "CTL1 : 37", "GPIF_NRD", "PC1 : 16", "->", "FX2 read strobe, low = STM32 must drive bus"),
        ("nSYNC", "CTL2 : 38", "GPIF_NSYNC", "PC4 : 33", "->", "low pulse = drop partial frames (FIFO reset)"),
        ("ACK", "RDY0 : 8", "GPIF_ACK", "PC2 : 17", "<-", "byte handshake (4-phase)"),
        ("DAV", "RDY1 : 9", "GPIF_DAV", "PC3 : 18", "<-", "STM32 has a 512-byte frame for the PC"),
        ("NRST", "PA0 : 40", "STM_NRST_CTL", "NRST : 14", "->", "FX2 can reset the STM32 (vendor req. 0xB1)"),
    ]
    cols = [15, 40, 82, 118, 175, 188]
    y0 = 178
    sh.text(15, y0 - 5, "GPIF interconnect", size=3.0, bold=True)
    for r, row in enumerate(rows):
        yy = y0 + r * 5.2
        for ci, cell in enumerate(row):
            sh.text(cols[ci] + 1, yy + 3.5, cell, size=2.1, bold=(r == 0))
        sh.line(15, yy + 5.2, 270, yy + 5.2, S.C_GREY, 0.15)
    sh.rect(15, y0, 255, len(rows) * 5.2, color=S.C_GREY, width=0.25)

    sh.note_box(285, 175, 120, [
        "1. One GPIF transaction = one byte, fully handshaked:",
        "   the STM32 serves the bus by GPIO polling, no timing constraint.",
        "2. USB data never passes through the 8051 (AUTOOUT/AUTOIN FIFOs).",
        "3. All transfers are 512-byte frames (one HS packet).",
        "4. VID/PID 04B4:1004 (Cypress driver) is for development only.",
        "5. Decouple every VCC/VDD pin with 100 nF placed at the pin.",
        "6. USB D+/D-: 90 ohm differential, short, no stubs, no vias",
        "   if possible; keep the 24 MHz crystal next to U1.",
        "7. Keep FD/CTL/RDY traces < 10 cm, series resistors at U1.",
        "8. Unused FX2 PA2..PA7 and PD0..PD7 are driven low by firmware.",
    ], title="Design notes")
    return sh


# =============================================================================
# Sheet 2 - USB connector, ESD, power
# =============================================================================
def sheet_power(c):
    sh = new_sheet(c, 2, "USB connector & power supply")

    sh.text(20, 22, "USB 2.0 connector", size=3.0, bold=True)
    sh.connector("J1", "USB_B_Micro", 30, 30, [
        (1, "VBUS", "VBUS_CONN"), (2, "D-", "USB_DM"), (3, "D+", "USB_DP"),
        (4, "ID", None), (5, "GND", "GND"), (6, "SHIELD", "USB_SHIELD")],
        footprint="USB_Micro-B_Molex_105017-0001", descr="USB Micro-B receptacle",
        mpn="Molex 105017-0001")

    # shield: 1M || 4.7nF to GND
    sh.two("R", "R1", "1M", 75, 62, "USB_SHIELD", "GND", footprint=FP_0603)
    sh.two("C", "C4", "4.7nF", 105, 62, "USB_SHIELD", "GND", footprint=FP_0603)

    # ESD
    sh.text(130, 22, "ESD protection (place next to J1)", size=3.0, bold=True)
    sh.ic("U4", "USBLC6-2SC6", 150, 36, 26,
          [(1, "I/O1", "USB_DP"), (6, "I/O1", "USB_DP"), (3, "I/O2", "USB_DM"), (4, "I/O2", "USB_DM")],
          [(5, "VBUS", "VBUS_CONN"), (2, "GND", "GND")],
          pitch=5.0, footprint="SOT-23-6", descr="USB ESD protection", mpn="ST USBLC6-2SC6")

    # input filter
    sh.text(20, 98, "VBUS input", size=3.0, bold=True)
    sh.two("F", "F1", "500mA PTC", 35, 112, "VBUS_CONN", "VBUS_F", orient="h", length=22,
           footprint="Fuse_1206", descr="resettable fuse", mpn="Bourns MF-MSMF050-2",
           attach=(True, False))
    sh.two("FB", "FB1", "600R@100MHz", 57, 112, "VBUS_F", "+5V", orient="h", length=22,
           footprint=FP_0805, descr="ferrite bead 1A", mpn="Murata BLM21PG601SN1",
           attach=(False, True))
    sh.dot(57, 112)
    sh.tp("TP1", 100, 122, "+5V")
    sh.line(100, 122, 100, 126)
    sh.label(100, 126, "+5V", "r")

    # regulator
    sh.text(130, 98, "3.3 V regulator (AMS1117-3.3, 1 A)", size=3.0, bold=True)
    sh.ic("U3", "AMS1117-3.3", 165, 106, 28,
          [(3, "VIN", "+5V"), (1, "GND", "GND")],
          [(2, "VOUT", "+3V3"), (4, "TAB", "+3V3")],
          pitch=6.0, footprint="SOT-223-3_TabPin2", descr="LDO 3.3V 1A", mpn="AMS1117-3.3")
    sh.cap_row(140, 140, [("C1", "10uF", "0805")], "+5V")
    sh.cap_row(215, 140, [("C2", "22uF tant", "Tantalum_3528"), ("C3", "100nF", FP_0603)], "+3V3")
    sh.text(140, 165, "C1: VBUS bulk must stay <= 10 uF (USB 2.0 inrush limit)", size=1.9, color=S.C_GREY)
    sh.text(215, 165, "C2: AMS1117 needs >= 22 uF tantalum for stability", size=1.9, color=S.C_GREY)
    sh.tp("TP2", 260, 135, "+3V3")
    sh.line(260, 135, 260, 139)
    sh.label(260, 139, "+3V3", "r")

    # power LED
    sh.text(300, 98, "Power LED", size=3.0, bold=True)
    sh.two("R", "R2", "1k", 310, 108, "+3V3", "PWR_LED_A", footprint=FP_0603, attach=(True, False))
    sh.two("LED", "D1", "green", 310, 122, "PWR_LED_A", "GND", footprint="LED_0603", descr="LED green")
    sh.dot(310, 122)

    sh.tp("TP3", 340, 130, "GND")
    sh.gnd(340, 130)

    sh.note_box(20, 185, 180, [
        "Board is bus powered: FX2LP <= 85 mA, STM32 <= 50 mA, LEDs ~ 8 mA",
        "-> bMaxPower = 250 mA in the USB configuration descriptor.",
        "USB_SHIELD is tied to GND through 1M || 4.7 nF near J1.",
        "U4 must sit between J1 and U1 with straight-through D+/D- routing.",
    ], title="Notes")
    return sh


# =============================================================================
# Sheet 3 - CY7C68013A
# =============================================================================
def sheet_fx2(c):
    sh = new_sheet(c, 3, "CY7C68013A-56PVXC (EZ-USB FX2LP)")

    left = [
        (6, "VCC", "+3V3"), (18, "VCC", "+3V3"), (24, "VCC", "+3V3"), (34, "VCC", "+3V3"),
        (39, "VCC", "+3V3"), (50, "VCC", "+3V3"), None,
        (10, "AVCC", "+3V3_FXA"), (14, "AVCC", "+3V3_FXA"), None,
        (12, "XTALIN", "FX_XI"), (11, "XTALOUT", "FX_XO"), None,
        (15, "DPLUS", "USB_DP"), (16, "DMINUS", "USB_DM"), None,
        (22, "SCL", "I2C_SCL"), (23, "SDA", "I2C_SDA"), None,
        (49, "RESET#", "FX_RESET_N"), (51, "WAKEUP", "FX_WAKEUP"), (21, "RESERVED", "GND"),
        (20, "IFCLK", "FX_IFCLK"), (5, "CLKOUT", "FX_CLKOUT"), None,
        (4, "GND", "GND"), (7, "GND", "GND"), (19, "GND", "GND"), (33, "GND", "GND"),
        (35, "GND", "GND"), (48, "GND", "GND"), (13, "AGND", "GND"), (17, "AGND", "GND"),
    ]
    right = [(25 + i, "PB%d/FD%d" % (i, i), "FD%d" % i) for i in range(8)]
    pd_pins = [52, 53, 54, 55, 56, 1, 2, 3]
    right += [(pd_pins[i], "PD%d" % i, None) for i in range(8)]
    right += [None,
              (36, "CTL0", "GPIF_NWR"), (37, "CTL1", "GPIF_NRD"), (38, "CTL2", "GPIF_NSYNC"),
              (8, "RDY0", "GPIF_ACK"), (9, "RDY1", "GPIF_DAV"), None,
              (40, "PA0/INT0#", "STM_NRST_CTL"), (41, "PA1/INT1#", "FX_LED_N"),
              (42, "PA2/SLOE", None), (43, "PA3/WU2", None), (44, "PA4/FIFOADR0", None),
              (45, "PA5/FIFOADR1", None), (46, "PA6/PKTEND", None), (47, "PA7/FLAGD", None)]
    sh.ic("U1", "CY7C68013A-56PVXC", 165, 24, 62, left, right, pitch=3.6,
          footprint="SSOP-56_7.5x18.5mm_P0.635mm", descr="EZ-USB FX2LP USB 2.0 controller",
          mpn="Infineon CY7C68013A-56PVXC",
          top_text="PA2..PA7, PD0..PD7 (unused in 8-bit mode): driven low by firmware, not floating")

    # crystal
    sh.text(20, 22, "24 MHz crystal", size=3.0, bold=True)
    sh.two("XTAL", "Y1", "24MHz", 40, 36, "FX_XI", "FX_XO", orient="h", length=22,
           footprint="Crystal_SMD_HC49-SD", descr="24 MHz fundamental, CL 12 pF",
           mpn="ABLS-24.000MHZ-B4-T")
    sh.two("C", "C10", "12pF", 40, 36, "FX_XI", "GND", attach=(False, True), footprint=FP_0603)
    sh.two("C", "C11", "12pF", 62, 36, "FX_XO", "GND", attach=(False, True), footprint=FP_0603)
    sh.dot(40, 36)
    sh.dot(62, 36)

    # reset
    sh.text(20, 70, "Reset (> 5 ms after VCC)", size=3.0, bold=True)
    sh.two("R", "R10", "10k", 30, 80, "+3V3", "FX_RESET_N", footprint=FP_0603, attach=(True, False))
    sh.two("C", "C12", "1uF", 30, 94, "FX_RESET_N", "GND", footprint=FP_0603, attach=(False, True))
    sh.two("SW", "SW1", "RESET", 46, 94, "FX_RESET_N", "GND", footprint="SW_SPST_TL3342",
           descr="tactile switch", attach=(False, True))
    sh.line(30, 94, 46, 94)
    sh.dot(30, 94)
    sh.dot(46, 94)
    sh.wire_label(46, 94, 58, "FX_RESET_N", "r")

    # wakeup / ifclk / clkout
    sh.text(20, 125, "Misc. pins", size=3.0, bold=True)
    sh.two("R", "R11", "10k", 30, 135, "+3V3", "FX_WAKEUP", footprint=FP_0603)
    sh.two("R", "R12", "10k", 75, 135, "FX_IFCLK", "GND", footprint=FP_0603)
    sh.tp("TP10", 95, 145, "FX_IFCLK")
    sh.line(95, 145, 95, 149)
    sh.label(95, 149, "FX_IFCLK", "r")
    sh.tp("TP11", 95, 165, "FX_CLKOUT")
    sh.line(95, 165, 95, 169)
    sh.label(95, 169, "FX_CLKOUT", "r")
    sh.text(20, 178, "IFCLK pin is not driven (IFCONFIG.5 = 0), R12 keeps it defined.",
            size=1.8, color=S.C_GREY)

    # EEPROM
    sh.text(290, 22, "Boot EEPROM (C2 load, address 0x51)", size=3.0, bold=True)
    sh.two("R", "R13", "2.2k", 300, 32, "+3V3", "I2C_SCL", footprint=FP_0603)
    sh.two("R", "R14", "2.2k", 322, 32, "+3V3", "I2C_SDA", footprint=FP_0603)
    sh.cap_row(370, 34, [("C13", "100nF", FP_0603)], "+3V3")
    sh.text(362, 56, "C13 at U2 pin 8", size=1.8, color=S.C_GREY)
    sh.ic("U2", "24LC128-I/SN", 335, 64, 26,
          [(1, "A0", "+3V3"), (2, "A1", "GND"), (3, "A2", "GND"), (4, "VSS", "GND")],
          [(8, "VCC", "+3V3"), (7, "WP", "GND"), (6, "SCL", "I2C_SCL"), (5, "SDA", "EE_SDA")],
          pitch=4.0, footprint="SOIC-8_3.9x4.9mm_P1.27mm", descr="128 kbit I2C EEPROM",
          mpn="Microchip 24LC128-I/SN")
    sh.two("JMP", "JP1", "EEPROM", 325, 102, "I2C_SDA", "EE_SDA", orient="h", length=16,
           footprint="PinHeader_1x02_P2.54mm", descr="jumper, open = boot without EEPROM")
    sh.text(300, 111, "JP1 open: FX2 boots without EEPROM and enumerates", size=1.8, color=S.C_GREY)
    sh.text(300, 114.5, "as 04B4:8613 (first programming / recovery).", size=1.8, color=S.C_GREY)

    # activity LED
    sh.text(290, 128, "FX2 activity LED (PA1, active low)", size=3.0, bold=True)
    sh.two("R", "R15", "1k", 305, 138, "+3V3", "FX_LED_A", footprint=FP_0603, attach=(True, False))
    sh.two("LED", "D2", "red", 305, 152, "FX_LED_A", "FX_LED_N", footprint="LED_0603", descr="LED red")
    sh.dot(305, 152)

    # STM32 reset control
    sh.text(290, 182, "STM32 reset from FX2 (PA0)", size=3.0, bold=True)
    sh.two("R", "R16", "100R", 320, 196, "STM_NRST_CTL", "STM_NRST", orient="h", length=16,
           footprint=FP_0603)
    sh.text(290, 205, "PA0 is only ever driven low (OEA.0 = 1 with IOA.0 = 0):", size=1.8, color=S.C_GREY)
    sh.text(290, 208.5, "open-drain emulation, no conflict with SW2 / ST-Link.", size=1.8, color=S.C_GREY)

    # decoupling
    sh.text(20, 192, "Decoupling (one 100 nF per VCC pin, at the pin)", size=3.0, bold=True)
    sh.cap_row(30, 202, [("C14", "100nF", FP_0603), ("C15", "100nF", FP_0603), ("C16", "100nF", FP_0603),
                         ("C17", "100nF", FP_0603), ("C18", "100nF", FP_0603), ("C19", "100nF", FP_0603),
                         ("C20", "10uF", FP_0805)], "+3V3")
    sh.text(120, 192, "Analog supply AVCC", size=3.0, bold=True)
    sh.two("FB", "FB2", "600R@100MHz", 130, 202, "+3V3", "+3V3_FXA", orient="h", length=16,
           footprint=FP_0805, mpn="Murata BLM18PG601SN1")
    sh.cap_row(175, 212, [("C21", "1uF", FP_0603), ("C22", "100nF", FP_0603), ("C23", "100nF", FP_0603)],
               "+3V3_FXA")
    return sh


# =============================================================================
# Sheet 4 - STM32F103VET6
# =============================================================================
STM_PORTS = {
    "PA": [23, 24, 25, 26, 29, 30, 31, 32, 67, 68, 69, 70, 71, 72, 76, 77],
    "PB": [35, 36, 37, 89, 90, 91, 92, 93, 95, 96, 47, 48, 51, 52, 53, 54],
    "PC": [15, 16, 17, 18, 33, 34, 63, 64, 65, 66, 78, 79, 80, 7, 8, 9],
    "PD": [81, 82, 83, 84, 85, 86, 87, 88, 55, 56, 57, 58, 59, 60, 61, 62],
    "PE": [97, 98, 1, 2, 3, 4, 5, 38, 39, 40, 41, 42, 43, 44, 45, 46],
}
STM_NETS = {
    "PA9": ("PA9/USART1_TX", "UART_TX"), "PA10": ("PA10/USART1_RX", "UART_RX"),
    "PA13": ("PA13/SWDIO", "SWDIO"), "PA14": ("PA14/SWCLK", "SWCLK"),
    "PB2": ("PB2/BOOT1", "STM_BOOT1"),
    "PC0": ("PC0  nWR", "GPIF_NWR"), "PC1": ("PC1  nRD", "GPIF_NRD"),
    "PC2": ("PC2  ACK", "GPIF_ACK"), "PC3": ("PC3  DAV", "GPIF_DAV"),
    "PC4": ("PC4  nSYNC", "GPIF_NSYNC"),
    "PD12": ("PD12 LED", "LED_HB"), "PD13": ("PD13 LED", "LED_USR"),
}
for i in range(8):
    STM_NETS["PE%d" % i] = ("PE%d  D%d" % (i, i), "GD%d" % i)


def stm_port(port):
    out = []
    for i, pin in enumerate(STM_PORTS[port]):
        name = "%s%d" % (port, i)
        label, net = STM_NETS.get(name, (name, None))
        out.append((pin, label, net))
    return out


def sheet_stm32(c):
    sh = new_sheet(c, 4, "STM32F103VET6 (GPIF slave)")

    left = [(11, "VDD_5", "+3V3"), (28, "VDD_4", "+3V3"), (50, "VDD_1", "+3V3"), (75, "VDD_2", "+3V3"),
            (100, "VDD_3", "+3V3"), (6, "VBAT", "+3V3"), (22, "VDDA", "+3V3_STA"), (21, "VREF+", "+3V3_STA"),
            None,
            (10, "VSS_5", "GND"), (27, "VSS_4", "GND"), (49, "VSS_1", "GND"), (74, "VSS_2", "GND"),
            (99, "VSS_3", "GND"), (19, "VSSA", "GND"), (20, "VREF-", "GND"), None,
            (12, "OSC_IN", "ST_XI"), (13, "OSC_OUT", "ST_XO"), (14, "NRST", "STM_NRST"),
            (94, "BOOT0", "STM_BOOT0"), (73, "NC", None), None] + stm_port("PC")
    right = stm_port("PE") + [None] + stm_port("PA") + [None] + stm_port("PB")
    left = left + [None] + stm_port("PD")
    sh.ic("U5", "STM32F103VET6", 175, 18, 58, left, right, pitch=3.3,
          footprint="LQFP-100_14x14mm_P0.5mm", descr="Cortex-M3 MCU 512K flash 64K RAM",
          mpn="ST STM32F103VET6")

    # crystal
    sh.text(20, 22, "8 MHz crystal (HSE)", size=3.0, bold=True)
    sh.two("XTAL", "Y2", "8MHz", 40, 36, "ST_XI", "ST_XO", orient="h", length=22,
           footprint="Crystal_SMD_HC49-SD", descr="8 MHz, CL 20 pF", mpn="ABLS-8.000MHZ-B4-T")
    sh.two("C", "C30", "20pF", 40, 36, "ST_XI", "GND", attach=(False, True), footprint=FP_0603)
    sh.two("C", "C31", "20pF", 62, 36, "ST_XO", "GND", attach=(False, True), footprint=FP_0603)
    sh.dot(40, 36)
    sh.dot(62, 36)

    # reset
    sh.text(20, 70, "Reset", size=3.0, bold=True)
    sh.two("R", "R30", "10k", 30, 80, "+3V3", "STM_NRST", footprint=FP_0603, attach=(True, False))
    sh.two("C", "C32", "100nF", 30, 94, "STM_NRST", "GND", footprint=FP_0603, attach=(False, True))
    sh.two("SW", "SW2", "RESET", 46, 94, "STM_NRST", "GND", footprint="SW_SPST_TL3342",
           descr="tactile switch", attach=(False, True))
    sh.line(30, 94, 46, 94)
    sh.dot(30, 94)
    sh.dot(46, 94)
    sh.wire_label(46, 94, 58, "STM_NRST", "r")

    # boot
    sh.text(20, 125, "Boot mode (BOOT0 = 0: flash)", size=3.0, bold=True)
    sh.two("R", "R31", "10k", 30, 145, "STM_BOOT0", "GND", footprint=FP_0603)
    sh.two("JMP", "JP2", "BOOT0", 75, 140, "+3V3", "STM_BOOT0", orient="h", length=16,
           footprint="PinHeader_1x02_P2.54mm", descr="jumper, closed = system bootloader")
    sh.two("R", "R32", "10k", 30, 175, "STM_BOOT1", "GND", footprint=FP_0603)

    # SWD + UART
    sh.text(300, 20, "Debug connectors", size=3.0, bold=True)
    sh.connector("J2", "SWD 1x5 2.54mm", 300, 28, [
        (1, "VTref", "+3V3"), (2, "SWCLK", "SWCLK"), (3, "GND", "GND"),
        (4, "SWDIO", "SWDIO"), (5, "NRST", "STM_NRST")], footprint="PinHeader_1x05_P2.54mm",
        descr="ST-Link SWD header")
    sh.connector("J3", "UART 1x3 2.54mm", 362, 28, [
        (1, "GND", "GND"), (2, "TX", "UART_TX"), (3, "RX", "UART_RX")],
        footprint="PinHeader_1x03_P2.54mm", descr="debug UART (3.3 V levels)")

    # LEDs
    sh.text(300, 70, "LEDs (active high)", size=3.0, bold=True)
    sh.two("R", "R33", "1k", 300, 82, "LED_HB", "LED_HB_A", orient="h", length=14,
           footprint=FP_0603, attach=(True, False))
    sh.two("LED", "D4", "green", 314, 82, "LED_HB_A", "GND", footprint="LED_0603", descr="heartbeat",
           attach=(False, True))
    sh.dot(314, 82)
    sh.two("R", "R34", "1k", 345, 82, "LED_USR", "LED_USR_A", orient="h", length=14,
           footprint=FP_0603, attach=(True, False))
    sh.two("LED", "D5", "yellow", 359, 82, "LED_USR_A", "GND", footprint="LED_0603", descr="user LED",
           attach=(False, True))
    sh.dot(359, 82)

    # decoupling
    sh.text(300, 120, "Decoupling (100 nF at each VDD pin)", size=3.0, bold=True)
    sh.cap_row(310, 130, [("C33", "100nF", FP_0603), ("C34", "100nF", FP_0603), ("C35", "100nF", FP_0603),
                          ("C36", "100nF", FP_0603), ("C37", "100nF", FP_0603), ("C38", "100nF", FP_0603),
                          ("C39", "10uF", FP_0805)], "+3V3")
    sh.text(300, 160, "C38: VBAT", size=1.8, color=S.C_GREY)
    sh.text(300, 175, "Analog supply VDDA / VREF+", size=3.0, bold=True)
    sh.two("FB", "FB3", "600R@100MHz", 300, 188, "+3V3", "+3V3_STA", orient="h", length=16,
           footprint=FP_0805, mpn="Murata BLM18PG601SN1")
    sh.cap_row(345, 198, [("C40", "1uF", FP_0603), ("C41", "100nF", FP_0603)], "+3V3_STA")

    sh.note_box(20, 205, 120, [
        "Unused GPIOs (X) stay in reset state (floating input);",
        "firmware may set them to analog mode for lowest power.",
        "PA13/PA14 remain SWD (default after reset).",
        "PE[7:0] switch between input and output at run time;",
        "the 33R series resistors (sheet 5) limit any contention.",
    ], title="Notes")
    return sh


# =============================================================================
# Sheet 5 - GPIF interconnect + timing
# =============================================================================
def wave(sh, x, y, w, name, segs, high=None):
    """segs: list of (duration, level) level in 'H','L','Z','D' (data) or text"""
    sh.text(x - 2, y + 1.5, name, size=2.2, anchor="r", bold=True)
    hh = 4.0
    xx = x
    total = sum(d for d, _ in segs)
    scale = w / total
    prev = None
    for d, lv in segs:
        x2 = xx + d * scale
        if lv == "H":
            sh.line(xx, y - hh / 2, x2, y - hh / 2, S.C_WIRE, 0.3)
            if prev == "L":
                sh.line(xx, y + hh / 2, xx, y - hh / 2, S.C_WIRE, 0.3)
        elif lv == "L":
            sh.line(xx, y + hh / 2, x2, y + hh / 2, S.C_WIRE, 0.3)
            if prev == "H":
                sh.line(xx, y - hh / 2, xx, y + hh / 2, S.C_WIRE, 0.3)
        elif lv == "Z":
            sh.line(xx, y, x2, y, S.C_GREY, 0.3)
        else:
            sh.poly([(xx, y), (xx + 1, y - hh / 2), (x2 - 1, y - hh / 2), (x2, y),
                     (x2 - 1, y + hh / 2), (xx + 1, y + hh / 2)], color=S.C_WIRE, width=0.3)
            sh.text((xx + x2) / 2, y + 0.8, lv, size=1.8, anchor="c")
        prev = lv
        xx = x2


def sheet_gpif(c):
    sh = new_sheet(c, 5, "GPIF interconnect & handshake timing")

    sh.text(20, 20, "Data bus series resistors (place close to U1)", size=3.0, bold=True)
    for k in range(2):
        sh.rarray("RN%d" % (k + 1), "4x33R", 50, 34 + k * 26,
                  ["FD%d" % (4 * k + i) for i in range(4)],
                  ["GD%d" % (4 * k + i) for i in range(4)])

    sh.text(130, 20, "Handshake line bias (defined levels while a chip is in reset)", size=3.0, bold=True)
    sh.two("R", "R50", "10k", 140, 32, "+3V3", "GPIF_NWR", footprint=FP_0603)
    sh.two("R", "R51", "10k", 160, 32, "+3V3", "GPIF_NRD", footprint=FP_0603)
    sh.two("R", "R52", "10k", 180, 32, "+3V3", "GPIF_NSYNC", footprint=FP_0603)
    sh.two("R", "R53", "10k", 210, 32, "GPIF_ACK", "GND", footprint=FP_0603)
    sh.two("R", "R54", "10k", 235, 32, "GPIF_DAV", "GND", footprint=FP_0603)
    sh.text(130, 64, "Pull-ups keep strobes inactive, pull-downs keep ACK/DAV = 0", size=1.9, color=S.C_GREY)
    sh.text(130, 67.5, "(FX2 never sees 'ready' from an STM32 that is in reset).", size=1.9, color=S.C_GREY)

    sh.text(130, 80, "Logic analyser header", size=3.0, bold=True)
    sh.connector("J4", "1x6 2.54mm", 140, 86, [
        (1, "nWR", "GPIF_NWR"), (2, "nRD", "GPIF_NRD"), (3, "ACK", "GPIF_ACK"),
        (4, "DAV", "GPIF_DAV"), (5, "nSYNC", "GPIF_NSYNC"), (6, "GND", "GND")],
        footprint="PinHeader_1x06_P2.54mm", descr="test header")

    # pin map table
    sh.text(270, 20, "Bus mapping", size=3.0, bold=True)
    rows = [("FX2 pin", "FX2 signal", "net (FX2 / STM32)", "STM32 pin")]
    fx_pins = [25, 26, 27, 28, 29, 30, 31, 32, 52, 53, 54, 55, 56, 1, 2, 3]
    for i in range(8):
        port = "PB%d" % i
        rows.append((str(fx_pins[i]), "%s / FD%d" % (port, i), "FD%d / GD%d" % (i, i),
                     "%d  PE%d" % (STM_PORTS["PE"][i], i)))
    for k, r in enumerate(rows):
        yy = 26 + k * 4.4
        for ci, (cx, cell) in enumerate(zip((272, 290, 318, 355), r)):
            sh.text(cx, yy + 3.2, cell, size=2.0, bold=(k == 0))
        sh.line(270, yy + 4.4, 380, yy + 4.4, S.C_GREY, 0.12)
    sh.rect(270, 26, 110, len(rows) * 4.4, color=S.C_GREY, width=0.25)

    # timing diagrams
    sh.text(20, 150, "FX2 -> STM32 byte write (GPIF FIFO-write waveform, 1 transaction)", size=3.0, bold=True)
    X, W = 45, 160
    wave(sh, X, 162, W, "FD[7:0]", [(1, "Z"), (8, "byte n (FX2 drives)"), (6, "byte n+1 (after NEXT)")])
    wave(sh, X, 172, W, "nWR", [(3, "H"), (6, "L"), (6, "H")])
    wave(sh, X, 182, W, "ACK", [(6, "L"), (5, "H"), (4, "L")])
    for k, (xx, t) in enumerate(((X + 3 * W / 15, "S1: nWR=0, wait ACK"),
                                 (X + 6 * W / 15, "STM32 latches PE, ACK=1"),
                                 (X + 9 * W / 15, "S2: NEXT, nWR=1"),
                                 (X + 12 * W / 15, "STM32 ACK=0 -> IDLE"))):
        sh.line(xx, 157, xx, 186 + 3.5 * (k % 2), S.C_GREY, 0.12)
        sh.text(xx + 0.8, 189 + 3.5 * (k % 2), t, size=1.7, color=S.C_GREY)

    sh.text(20, 200, "STM32 -> FX2 byte read (GPIF FIFO-read waveform, 1 transaction)", size=3.0, bold=True)
    wave(sh, X, 212, W, "nRD", [(2, "H"), (7, "L"), (6, "H")])
    wave(sh, X, 222, W, "FD[7:0]", [(3, "Z"), (7, "STM32 drives byte"), (5, "Z")])
    wave(sh, X, 232, W, "ACK", [(5, "L"), (6, "H"), (4, "L")])
    for k, (xx, t) in enumerate(((X + 2 * W / 15, "S0: nRD=0, wait ACK"),
                                 (X + 5 * W / 15, "data valid, ACK=1"),
                                 (X + 8 * W / 15, "S1: sample"),
                                 (X + 9 * W / 15, "S2: nRD=1, STM32 releases bus"),
                                 (X + 11 * W / 15, "ACK=0 -> IDLE"))):
        sh.line(xx, 207, xx, 236 + 3.5 * (k % 2), S.C_GREY, 0.12)
        sh.text(xx + 0.8, 239 + 3.5 * (k % 2), t, size=1.7, color=S.C_GREY)

    sh.note_box(220, 150, 160, [
        "DAV (RDY1) is high while the STM32 has a 512-byte frame queued.",
        "FX2 reads one USB packet per GPIF trigger (512 bytes HS, 64 bytes FS)",
        "only when DAV = 1 and the EP6 FIFO is not full.",
        "FX2 writes one EP2 packet per trigger (byte count transactions).",
        "nSYNC low (1 ms): issued after power-up, SET_CONFIGURATION and",
        "vendor request 0xB1 - STM32 drops partial frames on both sides.",
        "All RDY inputs are synchronised by the FX2 (IFCONFIG.ASYNC = 1),",
        "so no setup/hold requirement exists on the STM32 side.",
    ], title="Protocol")
    return sh


# =============================================================================
def main():
    pdf = BASE + "_Schematic.pdf"
    c = rl_canvas.Canvas(pdf, pagesize=(PAGE_W * mm, PAGE_H * mm))
    c.setTitle(PROJECT + " schematic")
    c.setAuthor("generated by gen_schematic.py")
    for fn in (sheet_overview, sheet_power, sheet_fx2, sheet_stm32, sheet_gpif):
        fn(c)
        c.showPage()
    c.save()

    # ---- checks -----------------------------------------------------------
    problems = nl.check(allow_single=())
    pin_counts = {"U1": 56, "U5": 100, "U2": 8, "U3": 4, "U4": 6}
    for ref, n in pin_counts.items():
        got = len(nl.parts[ref]["pins"])
        if got != n:
            problems.append("%s has %d pins placed, expected %d" % (ref, got, n))
        nums = sorted(int(p) for p in nl.parts[ref]["pins"])
        if nums != list(range(1, n + 1)):
            problems.append("%s pin numbers are not 1..%d" % (ref, n))

    # ---- netlist ---------------------------------------------------------
    with open(BASE + "_Netlist.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Net", "Ref", "Pin", "PinName"])
        for net in sorted(nl.nets):
            for ref, pin, name in sorted(nl.nets[net], key=lambda t: (t[0], t[1].zfill(4))):
                w.writerow([net, ref, pin, name])
        for ref, pin, name in nl.nc:
            w.writerow(["(no connect)", ref, pin, name])

    # ---- BOM -------------------------------------------------------------
    groups = defaultdict(list)
    for ref, p in nl.parts.items():
        key = (p["value"], p["footprint"], p["mpn"], p["descr"])
        groups[key].append(ref)

    def refkey(r):
        import re
        m = re.match(r"([A-Z]+)(\d+)", r)
        return (m.group(1), int(m.group(2)))

    with open(BASE + "_BOM.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Qty", "References", "Value", "Footprint", "Manufacturer P/N", "Description"])
        for key, refs in sorted(groups.items(), key=lambda kv: refkey(sorted(kv[1], key=refkey)[0])):
            refs = sorted(refs, key=refkey)
            w.writerow([len(refs), " ".join(refs), key[0], key[1], key[2], key[3]])

    print("written:", pdf)
    print("parts: %d, nets: %d, no-connect pins: %d" % (len(nl.parts), len(nl.nets), len(nl.nc)))
    if problems:
        print("\nCHECK FAILED:")
        for p in problems:
            print("  -", p)
        sys.exit(1)
    print("netlist check: OK (every net has >= 2 connections, IC pin sets complete)")


if __name__ == "__main__":
    main()
