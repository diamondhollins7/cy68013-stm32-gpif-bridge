"""
schematic_lib.py - tiny schematic drawing + netlist library (reportlab)

Coordinates are millimetres, origin at the TOP-LEFT of the sheet, y grows down.
Connections are made by net name (net labels / power ports), exactly like
global labels in Altium/KiCad. Every pin placed is recorded in a netlist so
the generator can check that the drawing is electrically consistent.
"""
from collections import defaultdict, OrderedDict

from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.pdfbase.pdfmetrics import stringWidth

PAGE_W, PAGE_H = 420.0, 297.0

C_LINE = colors.HexColor("#8B0000")      # component bodies
C_WIRE = colors.HexColor("#00008B")      # wires
C_PIN = colors.HexColor("#202020")
C_NET = colors.HexColor("#00008B")
C_PWR = colors.HexColor("#B00000")
C_TEXT = colors.black
C_GREY = colors.HexColor("#606060")
C_FILL = colors.HexColor("#FFFDE7")

POWER_NETS = set()


def is_power(net):
    return net in POWER_NETS or net == "GND"


class Netlist:
    def __init__(self):
        self.nets = defaultdict(list)           # net -> [(ref, pin, pinname)]
        self.parts = OrderedDict()              # ref -> dict
        self.nc = []                            # (ref, pin, name)

    def part(self, ref, value, footprint, descr="", mpn=""):
        if ref in self.parts:
            raise ValueError("duplicate reference " + ref)
        self.parts[ref] = dict(value=value, footprint=footprint, descr=descr,
                               mpn=mpn, pins={})

    def connect(self, ref, pin, name, net):
        pins = self.parts[ref]["pins"]
        if str(pin) in pins:
            raise ValueError("%s pin %s placed twice" % (ref, pin))
        pins[str(pin)] = (name, net)
        if net is None:
            self.nc.append((ref, pin, name))
        else:
            self.nets[net].append((ref, str(pin), name))

    def check(self, allow_single=()):
        problems = []
        for net, nodes in sorted(self.nets.items()):
            if len(nodes) < 2 and net not in allow_single:
                problems.append("net %s has only one connection: %s" % (net, nodes))
        return problems


class Sheet:
    def __init__(self, canvas, netlist, number, total, title, project, rev, date):
        self.c = canvas
        self.nl = netlist
        self.number, self.total, self.title = number, total, title
        self.project, self.rev, self.date = project, rev, date

    # ---- coordinate helpers -------------------------------------------------
    def P(self, x, y):
        return x * mm, (PAGE_H - y) * mm

    def line(self, x1, y1, x2, y2, color=C_WIRE, width=0.25):
        c = self.c
        c.setStrokeColor(color)
        c.setLineWidth(width * mm)
        c.line(*self.P(x1, y1), *self.P(x2, y2))

    def poly(self, pts, color=C_LINE, width=0.25, fill=None, close=True):
        c = self.c
        p = c.beginPath()
        p.moveTo(*self.P(*pts[0]))
        for q in pts[1:]:
            p.lineTo(*self.P(*q))
        if close:
            p.close()
        c.setStrokeColor(color)
        c.setLineWidth(width * mm)
        if fill is not None:
            c.setFillColor(fill)
        c.drawPath(p, stroke=1, fill=1 if fill is not None else 0)

    def rect(self, x, y, w, h, color=C_LINE, width=0.3, fill=None):
        c = self.c
        c.setStrokeColor(color)
        c.setLineWidth(width * mm)
        if fill is not None:
            c.setFillColor(fill)
        X, Y = self.P(x, y + h)
        c.rect(X, Y, w * mm, h * mm, stroke=1, fill=1 if fill is not None else 0)

    def circle(self, x, y, r, color=C_WIRE, fill=None, width=0.25):
        c = self.c
        c.setStrokeColor(color)
        c.setLineWidth(width * mm)
        if fill is not None:
            c.setFillColor(fill)
        c.circle(*self.P(x, y), r * mm, stroke=1, fill=1 if fill is not None else 0)

    def text(self, x, y, s, size=2.2, color=C_TEXT, anchor="l", bold=False, rot=0):
        """size in mm (cap height approx). anchor: l / c / r. y = baseline."""
        c = self.c
        c.saveState()
        c.setFillColor(color)
        c.setFont("Helvetica-Bold" if bold else "Helvetica", size * mm / 0.72)
        X, Y = self.P(x, y)
        c.translate(X, Y)
        if rot:
            c.rotate(rot)
        if anchor == "l":
            c.drawString(0, 0, s)
        elif anchor == "r":
            c.drawRightString(0, 0, s)
        else:
            c.drawCentredString(0, 0, s)
        c.restoreState()

    @staticmethod
    def text_width(s, size, bold=False):
        """width in mm of text drawn with text(size=size)"""
        pt = size * mm / 0.72
        return stringWidth(s, "Helvetica-Bold" if bold else "Helvetica", pt) / mm

    def dot(self, x, y):
        self.circle(x, y, 0.55, color=C_WIRE, fill=C_WIRE)

    # ---- frame / title block -------------------------------------------------
    def frame(self):
        self.rect(5, 5, PAGE_W - 10, PAGE_H - 10, color=C_TEXT, width=0.5)
        for i in range(8):
            x = 5 + (PAGE_W - 10) * (i + 0.5) / 8
            self.text(x, 4.2, str(i + 1), size=2, anchor="c", color=C_GREY)
            self.text(x, PAGE_H - 2.2, str(i + 1), size=2, anchor="c", color=C_GREY)
        for i, ch in enumerate("ABCDEF"):
            y = 5 + (PAGE_H - 10) * (i + 0.5) / 6
            self.text(2.6, y + 1, ch, size=2, anchor="c", color=C_GREY)
            self.text(PAGE_W - 2.6, y + 1, ch, size=2, anchor="c", color=C_GREY)
        # title block bottom-right
        x0, y0, w, h = PAGE_W - 135, PAGE_H - 33, 130, 28
        self.rect(x0, y0, w, h, color=C_TEXT, width=0.4)
        self.line(x0, y0 + 9, x0 + w, y0 + 9, C_TEXT, 0.3)
        self.line(x0, y0 + 18, x0 + w, y0 + 18, C_TEXT, 0.3)
        self.line(x0 + 85, y0 + 18, x0 + 85, y0 + h, C_TEXT, 0.3)
        self.text(x0 + 2, y0 + 3.4, "PROJECT", size=1.6, color=C_GREY)
        self.text(x0 + 2, y0 + 7.6, self.project, size=3.0, bold=True)
        self.text(x0 + 2, y0 + 12.4, "SHEET TITLE", size=1.6, color=C_GREY)
        self.text(x0 + 2, y0 + 16.6, self.title, size=3.0, bold=True)
        self.text(x0 + 2, y0 + 21.4, "DATE", size=1.6, color=C_GREY)
        self.text(x0 + 2, y0 + 25.8, self.date, size=2.4)
        self.text(x0 + 40, y0 + 21.4, "REV", size=1.6, color=C_GREY)
        self.text(x0 + 40, y0 + 25.8, self.rev, size=2.4)
        self.text(x0 + 87, y0 + 21.4, "SHEET", size=1.6, color=C_GREY)
        self.text(x0 + 87, y0 + 25.8, "%d of %d" % (self.number, self.total), size=2.4)

    # ---- power ports / labels ---------------------------------------------
    def gnd(self, x, y):
        """ground symbol hanging below (x, y)"""
        self.line(x, y, x, y + 2)
        self.line(x - 2.2, y + 2, x + 2.2, y + 2, C_WIRE, 0.35)
        self.line(x - 1.4, y + 2.8, x + 1.4, y + 2.8, C_WIRE, 0.35)
        self.line(x - 0.6, y + 3.6, x + 0.6, y + 3.6, C_WIRE, 0.35)

    def vcc(self, x, y, net):
        """power bar above (x, y)"""
        self.line(x, y, x, y - 2)
        self.line(x - 2, y - 2, x + 2, y - 2, C_PWR, 0.4)
        self.text(x, y - 3, net, size=1.9, color=C_PWR, anchor="c", bold=True)

    def label(self, x, y, net, side):
        """net label at a wire end. side = 'l' (text extends left) / 'r'."""
        color = C_PWR if is_power(net) else C_NET
        size = 1.9
        w = self.text_width(net, size) + 3.0
        h = 2.8
        if side == "r":
            pts = [(x, y), (x + 1.2, y - h / 2), (x + w, y - h / 2),
                   (x + w, y + h / 2), (x + 1.2, y + h / 2)]
            self.poly(pts, color=color, width=0.18)
            self.text(x + 1.6, y + 0.7, net, size=size, color=color)
        else:
            pts = [(x, y), (x - 1.2, y - h / 2), (x - w, y - h / 2),
                   (x - w, y + h / 2), (x - 1.2, y + h / 2)]
            self.poly(pts, color=color, width=0.18)
            self.text(x - 1.6, y + 0.7, net, size=size, color=color, anchor="r")

    def nc_mark(self, x, y):
        s = 0.9
        self.line(x - s, y - s, x + s, y + s, colors.red, 0.3)
        self.line(x - s, y + s, x + s, y - s, colors.red, 0.3)

    def attach(self, x, y, net, direction):
        """attach a net to the free end of a pin/wire at (x, y).
        direction: where the wire points: 'l', 'r', 'u', 'd'."""
        if net is None:
            self.nc_mark(x, y)
            return
        if direction in ("u", "d"):
            if net == "GND" and direction == "d":
                self.gnd(x, y)
            elif is_power(net) and net != "GND" and direction == "u":
                self.vcc(x, y, net)
            else:
                # vertical wire end with label: short stub + horizontal label
                yy = y + 2.5 if direction == "d" else y - 2.5
                self.line(x, y, x, yy)
                self.label(x, yy, net, "r")
        else:
            self.label(x, y, net, direction)

    # ---- integrated circuit ------------------------------------------------
    def ic(self, ref, value, x, y, w, left, right, pitch=3.6, pin_len=6.0,
           footprint="", descr="", mpn="", top_text=None):
        """left/right: lists of (pin_number, pin_name, net) or None for a gap."""
        self.nl.part(ref, value, footprint, descr, mpn)
        n = max(len(left), len(right))
        h = (n + 1) * pitch
        self.rect(x, y, w, h, color=C_LINE, width=0.35, fill=C_FILL)
        if self.text_width(ref, 2.6, True) + self.text_width(value, 2.4) + 3 < w:
            self.text(x, y - 1.5, ref, size=2.6, bold=True)
            self.text(x + w, y - 1.5, value, size=2.4, anchor="r")
        else:
            self.text(x, y - 5.0, ref, size=2.6, bold=True)
            self.text(x, y - 1.5, value, size=2.4)
        if top_text:
            self.text(x + w / 2, y + h + 3.2, top_text, size=1.9, anchor="c", color=C_GREY)
        for side, pins in (("l", left), ("r", right)):
            for i, p in enumerate(pins):
                if p is None:
                    continue
                num, name, net = p
                py = y + (i + 1) * pitch
                if side == "l":
                    x0, x1 = x - pin_len, x
                    self.line(x0, py, x1, py, C_PIN, 0.25)
                    self.text(x1 - 0.8, py - 0.5, str(num), size=1.6, anchor="r", color=C_GREY)
                    self.text(x1 + 1.0, py + 0.75, name, size=1.9)
                    self.attach(x0, py, net, "l")
                else:
                    x0, x1 = x + w, x + w + pin_len
                    self.line(x0, py, x1, py, C_PIN, 0.25)
                    self.text(x0 + 0.8, py - 0.5, str(num), size=1.6, color=C_GREY)
                    self.text(x0 - 1.0, py + 0.75, name, size=1.9, anchor="r")
                    self.attach(x1, py, net, "r")
                self.nl.connect(ref, num, name, net)
        return h

    # ---- two-terminal parts -------------------------------------------------
    def two(self, kind, ref, value, x, y, net1, net2, orient="v", length=14.0,
            footprint="", descr="", mpn="", show_value=True, attach=(True, True)):
        """kind: R C CP L FB LED D XTAL SW F JMP. Pin 1 = top / left end."""
        self.nl.part(ref, value, footprint, descr, mpn)
        L = length
        if orient == "v":
            a, b = (x, y), (x, y + L)
        else:
            a, b = (x, y), (x + L, y)
        self._body(kind, a, b, orient)
        # reference / value text
        if orient == "v":
            self.text(x + 3.2, y + L / 2 - 0.6, ref, size=2.0, bold=True)
            if show_value:
                self.text(x + 3.2, y + L / 2 + 2.2, value, size=1.9)
        else:
            self.text(x + L / 2, y - (6.2 if show_value else 3.0), ref, size=2.0, bold=True, anchor="c")
            if show_value:
                self.text(x + L / 2, y - 3.0, value, size=1.9, anchor="c")
        if kind in ("LED", "D"):
            p1, p2 = "A", "K"
        else:
            p1, p2 = "1", "2"
        self.nl.connect(ref, p1, p1, net1)
        self.nl.connect(ref, p2, p2, net2)
        if orient == "v":
            if attach[0]:
                self.attach(x, y, net1, "u")
            if attach[1]:
                self.attach(x, y + L, net2, "d")
        else:
            if attach[0]:
                self.attach(x, y, net1, "l")
            if attach[1]:
                self.attach(x + L, y, net2, "r")
        return a, b

    def wire_label(self, x1, y, x2, net, side="r"):
        """horizontal wire from x1 to x2 ending in a net label"""
        self.line(x1, y, x2, y)
        self.label(x2, y, net, side)

    def tp(self, ref, x, y, net, footprint="TestPoint_Pad_D1.5mm"):
        """single-pin test point drawn as a small circle above (x, y)"""
        self.nl.part(ref, "TP", footprint, "test point")
        self.nl.connect(ref, "1", "1", net)
        self.line(x, y, x, y - 2.5)
        self.circle(x, y - 3.3, 0.8, color=C_LINE)
        self.text(x + 1.5, y - 3.0, ref, size=1.8, bold=True)

    def rarray(self, ref, value, x, y, left_nets, right_nets, pitch=5.0,
               footprint="R_Array_Convex_4x0603"):
        """4-element resistor array, pins 1-8, 2-7, 3-6, 4-5"""
        self.nl.part(ref, value, footprint, "4x resistor array")
        L = 16.0
        self.rect(x + L / 2 - 4.5, y - 3.2, 9, 4 * pitch + 1.4, color=C_GREY, width=0.15)
        self.text(x + L / 2, y - 4.2, ref + "  " + value, size=1.9, anchor="c", bold=True)
        for i in range(4):
            yy = y + i * pitch
            self._body("R", (x, yy), (x + L, yy), "h")
            p1, p2 = i + 1, 8 - i
            self.text(x + 1, yy - 0.8, str(p1), size=1.5, color=C_GREY)
            self.text(x + L - 1, yy - 0.8, str(p2), size=1.5, color=C_GREY, anchor="r")
            self.nl.connect(ref, p1, str(p1), left_nets[i])
            self.nl.connect(ref, p2, str(p2), right_nets[i])
            self.attach(x, yy, left_nets[i], "l")
            self.attach(x + L, yy, right_nets[i], "r")

    def _body(self, kind, a, b, orient):
        (x1, y1), (x2, y2) = a, b
        if orient == "v":
            cx, cy = x1, (y1 + y2) / 2
        else:
            cx, cy = (x1 + x2) / 2, y1

        def T(dx, dy):
            # local coords: along = dx (pin1 -> pin2), across = dy
            if orient == "v":
                return cx + dy, cy + dx
            return cx + dx, cy + dy

        def seg(d1, e1, d2, e2, color=C_LINE, w=0.3):
            p, q = T(d1, e1), T(d2, e2)
            self.line(p[0], p[1], q[0], q[1], color, w)

        half = (abs(x2 - x1) + abs(y2 - y1)) / 2
        if kind in ("R", "F"):
            seg(-half, 0, -3, 0, C_WIRE, 0.25)
            seg(3, 0, half, 0, C_WIRE, 0.25)
            pts = [T(-3, -1), T(3, -1), T(3, 1), T(-3, 1)]
            self.poly(pts, color=C_LINE, width=0.3)
            if kind == "F":
                seg(-3, 0, 3, 0, C_LINE, 0.25)
        elif kind in ("C", "CP"):
            seg(-half, 0, -0.7, 0, C_WIRE, 0.25)
            seg(0.7, 0, half, 0, C_WIRE, 0.25)
            seg(-0.7, -2.2, -0.7, 2.2, C_LINE, 0.4)
            seg(0.7, -2.2, 0.7, 2.2, C_LINE, 0.4)
            if kind == "CP":
                p = T(-2.0, -1.8)
                self.text(p[0], p[1] + 0.6, "+", size=2, anchor="c", color=C_LINE)
        elif kind in ("L", "FB"):
            seg(-half, 0, -3, 0, C_WIRE, 0.25)
            seg(3, 0, half, 0, C_WIRE, 0.25)
            if kind == "FB":
                pts = [T(-3, -1.1), T(3, -1.1), T(3, 1.1), T(-3, 1.1)]
                self.poly(pts, color=C_LINE, width=0.3, fill=C_LINE)
            else:
                for i in range(3):
                    p = T(-2 + 2 * i, 0)
                    self.circle(p[0], p[1], 1.0, color=C_LINE)
        elif kind in ("LED", "D"):
            seg(-half, 0, -1.4, 0, C_WIRE, 0.25)
            seg(1.4, 0, half, 0, C_WIRE, 0.25)
            pts = [T(-1.4, -1.6), T(-1.4, 1.6), T(1.4, 0)]
            self.poly(pts, color=C_LINE, width=0.3)
            seg(1.4, -1.6, 1.4, 1.6, C_LINE, 0.35)
            if kind == "LED":
                for o in (0.0, 1.2):
                    p, q = T(-0.4 + o, -1.9), T(0.6 + o, -3.1)
                    self.line(p[0], p[1], q[0], q[1], C_LINE, 0.2)
        elif kind == "XTAL":
            seg(-half, 0, -1.6, 0, C_WIRE, 0.25)
            seg(1.6, 0, half, 0, C_WIRE, 0.25)
            seg(-1.6, -2, -1.6, 2, C_LINE, 0.35)
            seg(1.6, -2, 1.6, 2, C_LINE, 0.35)
            pts = [T(-0.9, -1.4), T(0.9, -1.4), T(0.9, 1.4), T(-0.9, 1.4)]
            self.poly(pts, color=C_LINE, width=0.3)
        elif kind == "SW":
            seg(-half, 0, -2.5, 0, C_WIRE, 0.25)
            seg(2.5, 0, half, 0, C_WIRE, 0.25)
            p = T(-2.5, 0)
            self.circle(p[0], p[1], 0.5, color=C_LINE)
            p = T(2.5, 0)
            self.circle(p[0], p[1], 0.5, color=C_LINE)
            seg(-2.5, -0.6, 2.7, -2.2, C_LINE, 0.3)
            seg(0.1, -1.4, 0.1, -3.2, C_LINE, 0.3)
            seg(-1.2, -3.2, 1.4, -3.2, C_LINE, 0.3)
        elif kind == "JMP":
            seg(-half, 0, -1.6, 0, C_WIRE, 0.25)
            seg(1.6, 0, half, 0, C_WIRE, 0.25)
            for d in (-1.6, 1.6):
                p = T(d, 0)
                self.circle(p[0], p[1], 0.6, color=C_LINE)
            seg(-1.6, -1.4, 1.6, -1.4, C_LINE, 0.3)
        else:
            raise ValueError(kind)

    # ---- connector (single row) -------------------------------------------
    def connector(self, ref, value, x, y, pins, side="r", pitch=3.6,
                  footprint="", descr="", mpn=""):
        """pins: list of (num, name, net). Body is a narrow box; wires leave on 'side'."""
        self.nl.part(ref, value, footprint, descr, mpn)
        w = max(self.text_width(n, 1.7) for _, n, _ in pins) + 7.0
        h = (len(pins) + 1) * pitch
        self.rect(x, y, w, h, color=C_LINE, width=0.35, fill=C_FILL)
        self.text(x, y - 1.5, ref, size=2.4, bold=True)
        self.text(x, y + h + 3.0, value, size=1.9)
        for i, (num, name, net) in enumerate(pins):
            py = y + (i + 1) * pitch
            if side == "r":
                self.line(x + w, py, x + w + 5, py, C_PIN)
                self.text(x + 1.0, py + 0.7, str(num), size=1.8)
                self.text(x + w - 1.0, py + 0.7, name, size=1.7, color=C_GREY, anchor="r")
                self.attach(x + w + 5, py, net, "r")
            else:
                self.line(x - 5, py, x, py, C_PIN)
                self.text(x + w - 1.0, py + 0.7, str(num), size=1.8, anchor="r")
                self.text(x + 1.0, py + 0.7, name, size=1.7, color=C_GREY)
                self.attach(x - 5, py, net, "l")
            self.nl.connect(ref, num, name, net)
        return h

    # ---- decoupling rows -----------------------------------------------------
    def cap_row(self, x, y, caps, top_net, bot_net="GND", spacing=11.0, length=10.0):
        """caps: list of (ref, value, footprint). Drawn vertically between a
        top rail (top_net) and a bottom rail (bot_net) joined by wires."""
        n = len(caps)
        for i, (ref, value, fp) in enumerate(caps):
            cx = x + i * spacing
            kind = "CP" if "uF" in value and ("22" in value or "47" in value) and "tant" in fp.lower() else "C"
            self.nl.part(ref, value, fp, "capacitor")
            self._body(kind, (cx, y), (cx, y + length), "v")
            self.text(cx + 1.6, y + length / 2 - 3.0, ref, size=1.7, bold=True)
            self.text(cx + 1.6, y + length / 2 + 4.6, value, size=1.6)
            self.nl.connect(ref, "1", "1", top_net)
            self.nl.connect(ref, "2", "2", bot_net)
            if i > 0:
                self.dot(cx, y) if i < n - 1 else None
                self.dot(cx, y + length) if i < n - 1 else None
        x_end = x + (n - 1) * spacing
        self.line(x, y, x_end, y)
        self.line(x, y + length, x_end, y + length)
        if n > 2:
            self.dot(x, y)
            self.dot(x, y + length)
        # rails to power ports
        self.line(x, y, x - 4, y)
        self.label(x - 4, y, top_net, "l") if not is_power(top_net) else self._pwr_left(x - 4, y, top_net)
        if bot_net == "GND":
            self.gnd(x_end, y + length)
        else:
            self.line(x_end, y + length, x_end + 4, y + length)
            self.label(x_end + 4, y + length, bot_net, "r")

    def _pwr_left(self, x, y, net):
        self.line(x, y, x, y - 2)
        self.line(x - 2, y - 2, x + 2, y - 2, C_PWR, 0.4)
        self.text(x, y - 3, net, size=1.9, color=C_PWR, anchor="c", bold=True)

    # ---- free text / notes ---------------------------------------------------
    def note_box(self, x, y, w, lines, title=None, size=2.0, lh=3.2):
        h = len(lines) * lh + (lh + 1.5 if title else 0) + 2.5
        self.rect(x, y, w, h, color=C_GREY, width=0.25)
        yy = y + 1.5
        if title:
            yy += lh
            self.text(x + 2, yy, title, size=size + 0.3, bold=True)
            yy += 1.5
        for ln in lines:
            yy += lh
            self.text(x + 2, yy, ln, size=size)
        return h
