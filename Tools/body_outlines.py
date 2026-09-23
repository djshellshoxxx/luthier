#!/usr/bin/env python3
"""Body outlines for the guitar illustration (spec/guitar-illustration.md 1, 4, 5, 9).

Each body style is authored here in millimetres as Bezier knots, resampled into
the point lists the renderer smooths with a closed centripetal Catmull-Rom
spline, and written to Source/UI/Guitar/BodyOutlines.h. Headstocks are authored
the same way and written to Source/UI/Guitar/HeadstockOutlines.h. The previewer draws
every style the same way the renderer will (same spline), with a neck, frets,
strings and ghost pickups / bridge for context, so a shape can be judged as a
guitar before any C++ is compiled.

    python Tools/body_outlines.py              headers + every preview PNG
    python Tools/body_outlines.py --no-png     header only
    python Tools/body_outlines.py --only a,b   previews for some styles only

Needs Pillow and shapely (python -m pip install --user pillow shapely).

Authoring frame (all mm): x along the guitar axis from the body's leftmost
point (the end nearest the headstock) toward the tail, y lateral with the bass
side negative (top of the screen) and treble positive, the strings on y = 0.
Hardware is usually placed with at(X, Y): X measured from the saddle toward the
headstock, as in the scene (section 1).
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
HEADER = os.path.join(ROOT, "Source", "UI", "Guitar", "BodyOutlines.h")
PREVIEWS = os.path.join(HERE, "outline_previews")

DEFAULT_H = 0.38        # Bezier handle length as a fraction of the chord; ~0.39 is circular


# ----------------------------------------------------------------------------------------------
# Geometry

def dist(a, b):
    return math.hypot(b[0] - a[0], b[1] - a[1])


def unit(deg):
    r = math.radians(deg)
    return (math.cos(r), math.sin(r))


def bez_pt(seg, t):
    p0, c1, c2, p3 = seg
    mt = 1.0 - t
    a, b, c, d = mt * mt * mt, 3 * mt * mt * t, 3 * mt * t * t, t * t * t
    return (a * p0[0] + b * c1[0] + c * c2[0] + d * p3[0],
            a * p0[1] + b * c1[1] + c * c2[1] + d * p3[1])


def dense(seg, n=120):
    return [bez_pt(seg, i / n) for i in range(n + 1)]


class K:
    """A knot: position, direction of travel (degrees, 0 = toward the tail, 90 = toward the
    treble side), and handle lengths as fractions of the chord to the neighbouring knot.
    `ao` gives a different outgoing direction (a corner). a=None: tangent from the neighbours.
    A handle of 0 on both sides of a segment makes it straight."""

    def __init__(self, x, y, a=None, h=None, hin=None, hout=None, ao=None):
        self.p = (float(x), float(y))
        self.a = a
        self.ao = a if ao is None else ao
        base = DEFAULT_H if h is None else h
        self.hin = base if hin is None else hin
        self.hout = base if hout is None else hout

    def mirrored(self):
        """Reflected across the string axis and traversed the other way."""
        return K(self.p[0], -self.p[1],
                 a=None if self.ao is None else 180.0 - self.ao,
                 ao=None if self.a is None else 180.0 - self.a,
                 hin=self.hout, hout=self.hin)


def mirror_half(half):
    """half runs over the bass side from a point on the axis at the neck end to a point on
    the axis at the tail; returns the whole symmetric closed knot list."""
    return list(half) + [k.mirrored() for k in reversed(half[1:-1])]


def segments(knots, closed=True):
    n = len(knots)
    for i, k in enumerate(knots):
        if k.a is None:
            if closed or 0 < i < n - 1:
                p, q = knots[i - 1].p, knots[(i + 1) % n].p
            elif i == 0:
                p, q = k.p, knots[1].p
            else:
                p, q = knots[i - 1].p, k.p
            ang = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
            k.a = k.ao = ang
    segs = []
    for i in range(n if closed else n - 1):
        a, b = knots[i], knots[(i + 1) % n]
        ch = dist(a.p, b.p)
        if a.hout == 0 and b.hin == 0:
            c1 = (a.p[0] + (b.p[0] - a.p[0]) / 3, a.p[1] + (b.p[1] - a.p[1]) / 3)
            c2 = (a.p[0] + (b.p[0] - a.p[0]) * 2 / 3, a.p[1] + (b.p[1] - a.p[1]) * 2 / 3)
        else:
            u1, u2 = unit(a.ao), unit(b.a)
            c1 = (a.p[0] + u1[0] * a.hout * ch, a.p[1] + u1[1] * a.hout * ch)
            c2 = (b.p[0] - u2[0] * b.hin * ch, b.p[1] - u2[1] * b.hin * ch)
        segs.append((a.p, c1, c2, b.p))
    return segs


def seg_tangent(seg, at_end):
    p0, c1, c2, p3 = seg
    if at_end:
        v = (p3[0] - c2[0], p3[1] - c2[1]) if dist(c2, p3) > 1e-6 else (p3[0] - c1[0], p3[1] - c1[1])
    else:
        v = (c1[0] - p0[0], c1[1] - p0[1]) if dist(p0, c1) > 1e-6 else (c2[0] - p0[0], c2[1] - p0[1])
    return math.degrees(math.atan2(v[1], v[0]))


def angle_diff(a, b):
    d = (b - a + 180.0) % 360.0 - 180.0
    return abs(d)


def resample(segs, closed=True, max_len=40.0, max_turn=24.0):
    """Points for the renderer's Catmull-Rom: every knot, plus extra points where the curve
    is long or turning. Returns (points, corners): a knot where the tangent jumps is flagged
    as a corner, which the spline turns sharply (Pt::corner)."""
    n = len(segs)
    corner = [False] * n
    for i in range(n):
        prev = segs[i - 1] if (closed or i > 0) else None
        if prev is not None and angle_diff(seg_tangent(prev, True), seg_tangent(segs[i], False)) > 12.0:
            corner[i] = True
    pts, flags = [], []
    for i, seg in enumerate(segs):
        d = dense(seg, 160)
        cum_len, cum_turn = [0.0], [0.0]
        for j in range(1, len(d)):
            cum_len.append(cum_len[-1] + dist(d[j - 1], d[j]))
            if j >= 2:
                a1 = math.atan2(d[j - 1][1] - d[j - 2][1], d[j - 1][0] - d[j - 2][0])
                a2 = math.atan2(d[j][1] - d[j - 1][1], d[j][0] - d[j - 1][0])
                cum_turn.append(cum_turn[-1] + angle_diff(math.degrees(a1), math.degrees(a2)))
            else:
                cum_turn.append(0.0)
        cost = [cum_len[j] / max_len + cum_turn[j] / max_turn for j in range(len(d))]
        k = max(1, int(math.ceil(cost[-1] - 1e-9)))
        seg_pts = [d[0]]
        if cum_turn[-1] >= 0.5:
            j = 0
            for m in range(1, k):
                target = cost[-1] * m / k
                while j < len(d) - 1 and cost[j + 1] < target:
                    j += 1
                seg_pts.append(d[j + 1])
        for m, p in enumerate(seg_pts):
            if m == 0 or (dist(p, pts[-1]) > 0.8 and dist(p, d[-1]) > 0.8):    # knots win over samples
                pts.append(p)
                flags.append(corner[i] if m == 0 else False)
    if not closed:
        pts.append(segs[-1][3])
        flags.append(False)
    out, out_flags = [pts[0]], [flags[0]]
    for p, f in zip(pts[1:], flags[1:]):
        if dist(p, out[-1]) > 0.8:
            out.append(p)
            out_flags.append(f)
        else:
            out_flags[-1] = out_flags[-1] or f
    if closed and dist(out[0], out[-1]) < 0.8:
        out_flags[0] = out_flags[0] or out_flags[-1]
        out.pop()
        out_flags.pop()
    return out, out_flags


# Centripetal Catmull-Rom (alpha 0.5) as cubic Beziers - the same maths the header's
# luthier::outlines::catmullRomToBezier() uses, so the previews match the renderer.
def cr_bezier(p0, p1, p2, p3, alpha=0.5):
    eps = 1e-4
    d1 = max(dist(p0, p1) ** alpha, eps)
    d2 = max(dist(p1, p2) ** alpha, eps)
    d3 = max(dist(p2, p3) ** alpha, eps)
    a1 = 2 * d1 * d1 + 3 * d1 * d2 + d2 * d2
    n1 = 3 * d1 * (d1 + d2)
    a2 = 2 * d3 * d3 + 3 * d3 * d2 + d2 * d2
    n2 = 3 * d3 * (d3 + d2)
    b1 = ((d1 * d1 * p2[0] - d2 * d2 * p0[0] + a1 * p1[0]) / n1,
          (d1 * d1 * p2[1] - d2 * d2 * p0[1] + a1 * p1[1]) / n1)
    b2 = ((d3 * d3 * p1[0] - d2 * d2 * p3[0] + a2 * p2[0]) / n2,
          (d3 * d3 * p1[1] - d2 * d2 * p3[1] + a2 * p2[1]) / n2)
    return (p1, b1, b2, p2)


def smooth(pts, closed=True, angular=False, per_seg=12, corners=None):
    """The curve the renderer draws through pts, as a dense polyline. At a corner point the
    far neighbour is replaced by the corner itself (P0 := P1, P3 := P2), as in the renderer."""
    n = len(pts)
    corners = corners or [False] * n
    if n < 2:
        return list(pts)
    if angular:
        out = list(pts)
        if closed:
            out.append(pts[0])
        return out
    out = []
    rng = range(n) if closed else range(n - 1)
    for i in rng:
        if closed:
            p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
            c1, c2 = corners[i], corners[(i + 1) % n]
        else:
            p1, p2 = pts[i], pts[i + 1]
            p0 = pts[i - 1] if i > 0 else (2 * p1[0] - p2[0], 2 * p1[1] - p2[1])
            p3 = pts[i + 2] if i + 2 < n else (2 * p2[0] - p1[0], 2 * p2[1] - p1[1])
            c1, c2 = corners[i], corners[i + 1]
        if c1:
            p0 = p1
        if c2:
            p3 = p2
        seg = cr_bezier(p0, p1, p2, p3)
        for j in range(per_seg):
            out.append(bez_pt(seg, j / per_seg))
    out.append(pts[0] if closed else pts[-1])
    return out


def deviation(a, b):
    """Hausdorff distance between two polylines, mm."""
    from shapely.geometry import LineString
    return LineString(a).hausdorff_distance(LineString(b))


def offset_ring(ring, d, round_mm=0.0):
    """The closed ring moved inward by d mm (negative buffer), as a list of points. With
    round_mm, thin parts are dropped and corners rounded to that radius (open then close)."""
    from shapely.geometry import Polygon
    poly = Polygon(ring).buffer(0).buffer(-d, join_style=1, quad_segs=16)
    if round_mm > 0:
        r = round_mm
        poly = poly.buffer(-r, quad_segs=16).buffer(2 * r, quad_segs=16).buffer(-r, quad_segs=16)
    if poly.geom_type == "MultiPolygon":
        poly = max(poly.geoms, key=lambda g: g.area)
    return list(poly.exterior.coords)[:-1]


def simplify_ring(ring, max_len=24.0, max_turn=14.0):
    """Evenly thinned points from a dense ring, for data the spline will smooth again."""
    n = len(ring)
    cost = [0.0]
    for i in range(1, n + 1):
        a, b, c = ring[(i - 2) % n], ring[(i - 1) % n], ring[i % n]
        t = angle_diff(math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])),
                       math.degrees(math.atan2(c[1] - b[1], c[0] - b[0])))
        cost.append(cost[-1] + dist(b, c) / max_len + t / max_turn)
    k = max(8, int(math.ceil(cost[-1])))
    out, j = [], 0
    for m in range(k):
        target = cost[-1] * m / k
        while j < n - 1 and cost[j + 1] < target:
            j += 1
        out.append(ring[j])
    return out


# ----------------------------------------------------------------------------------------------
# Styles

class Style:
    """One body style. length / width / depth: the intended size in mm (checked against the
    built outline; spec holds the size spec/guitar-illustration.md 4 gives, where it differs).
    saddle_x / neck_x: the saddle and the neck-and-fretboard end along x. outline /
    pickguard / arch: knots (points when angular). hole / upper_holes: (kind, centre, extent
    along the axis, extent across). ctx: preview-only context (scale, pickups, colours)."""

    def __init__(self, id, family, length, width, depth, saddle_x, neck_x, outline, angular=False,
                 pickguard=None, hole=("none", (0, 0), 0, 0), upper_holes=("none", (0, 0), 0, 0),
                 controls=(), selector=None, jack=(0, 0), straps=((0, 0), (0, 0)), arch=None,
                 arch_closed=True, spec=None, note="", ctx=None):
        self.id, self.family = id, family
        self.target_length, self.target_width, self.depth = length, width, depth
        self.saddle_x, self.neck_x = saddle_x, neck_x
        self.outline_knots, self.angular = outline, angular
        self.pickguard_knots = pickguard
        self.hole, self.upper_holes = hole, upper_holes
        self.controls, self.selector, self.jack, self.straps = list(controls), selector, jack, straps
        self.arch, self.arch_closed = arch, arch_closed
        self.spec, self.note = spec, note
        self.ctx = ctx or {}

    # --- built data ---
    def build(self):
        if self.angular:
            self.outline = [k.p for k in self.outline_knots]
            self.outline_corners = [False] * len(self.outline)
            self.outline_ref = self.outline + [self.outline[0]]
        else:
            segs = segments(self.outline_knots, True)
            self.outline, self.outline_corners = resample(segs, True)
            self.outline_ref = [p for s in segs for p in dense(s, 60)]
        self.outline_smooth = smooth(self.outline, True, self.angular, corners=self.outline_corners)
        self.error = deviation(self.outline_smooth, self.outline_ref)

        xs = [p[0] for p in self.outline_smooth]
        ys = [p[1] for p in self.outline_smooth]
        self.x0 = min(xs)
        self.length = max(xs) - self.x0
        self.width = 2 * max(abs(y) for y in ys)
        self.phys_width = max(ys) - min(ys)

        self.pickguard, self.pickguard_corners = None, None
        if self.pickguard_knots:
            if self.angular:
                self.pickguard = [k.p for k in self.pickguard_knots]
                self.pickguard_corners = [False] * len(self.pickguard)
            else:
                self.pickguard, self.pickguard_corners = resample(segments(self.pickguard_knots, True), True,
                                                                  max_len=44.0, max_turn=26.0)

        self.arch_pts, self.arch_corners = None, None
        if self.arch is not None:
            if isinstance(self.arch, (int, float)):
                ring = offset_ring(self.outline_smooth[:-1], float(self.arch), round_mm=9.0)
                self.arch_pts = simplify_ring(ring, max_len=34.0, max_turn=18.0)
                self.arch_corners = [False] * len(self.arch_pts)
                self.arch_closed = True
            elif self.angular:
                self.arch_pts = [k.p for k in self.arch]
                self.arch_corners = [False] * len(self.arch_pts)
            else:
                self.arch_pts, self.arch_corners = resample(segments(self.arch, self.arch_closed), self.arch_closed,
                                                            max_len=30.0)
        return self

    def check(self):
        """Warnings for hardware off the body, a pickguard over a hole, or no room for the
        bridge and pickups on the string axis."""
        from shapely.geometry import Point, Polygon, box
        body = Polygon(self.outline_smooth[:-1]).buffer(0)
        warn = []

        def inside(p, slack, what):
            if p is None:
                return
            d = body.exterior.distance(Point(p))
            if not body.contains(Point(p)) and d > slack:
                warn.append("%s at (%.0f, %.0f) is %.1f mm outside the body" % (what, p[0], p[1], d))

        for i, c in enumerate(self.controls):
            inside(c, 0, "control %d" % i)
            if body.contains(Point(c)) and body.exterior.distance(Point(c)) < 15:
                warn.append("control %d is %.1f mm from the edge" % (i, body.exterior.distance(Point(c))))
        inside(self.selector, 0, "selector")
        inside(self.jack, 3, "jack")
        for i, sb in enumerate(self.straps):
            inside(sb, 3, "strap %d" % i)
        if self.pickguard:
            pg = Polygon(smooth(self.pickguard, True, self.angular, corners=self.pickguard_corners)[:-1]).buffer(0)
            outside = pg.difference(body.union(box(-50, -30, self.neck_x, 30)))
            if outside.area > 30:
                warn.append("pickguard has %.0f mm2 off the body" % outside.area)
            kind, hc, hw, hh = self.hole
            if kind in ("round", "oval", "dShape"):
                from shapely import affinity
                h = affinity.scale(Point(hc).buffer(1.0, quad_segs=24), hw / 2, hh / 2)
                if pg.intersection(h).area > 5:
                    warn.append("pickguard covers the sound hole")
            if kind == "fHoles":
                for side in (1, -1):
                    f = box(hc[0] - hw / 2, side * hc[1] - hh / 2, hc[0] + hw / 2, side * hc[1] + hh / 2)
                    if pg.intersection(f).area > 0.25 * f.area:
                        warn.append("pickguard covers an f-hole")
        # the strip the bridge and pickups sit in must be on the body
        strip = box(self.neck_x + 8, -36, self.saddle_x + 25, 36)
        off = strip.difference(body)
        if off.area > 10:
            warn.append("%.0f mm2 of the pickup / bridge strip is off the body" % off.area)
        jx = self.joint_x()
        if jx is not None and not (jx <= self.neck_x):
            warn.append("neckU is ahead of the body joint")
        if abs(self.length - self.target_length) > 4 or abs(self.phys_width - self.target_width) > 5:
            warn.append("built %.0f x %.0f mm, meant %.0f x %.0f" % (self.length, self.phys_width,
                                                                   self.target_length, self.target_width))
        return warn

    def uv(self, p):
        return ((p[0] - self.x0) / self.length, p[1] / self.width)

    def at(self, X, Y):
        return (self.saddle_x - X, Y)

    def fret_of_x(self, x):
        X = self.saddle_x - x
        scale = self.ctx.get("scale", 648.0)
        return 12.0 * math.log2(scale / X) if X > 1 else float("inf")

    def joint_x(self):
        """Where the body edge crosses the string axis at the neck end."""
        best = None
        pts = self.outline_smooth
        for a, b in zip(pts, pts[1:]):
            if (a[1] <= 0 < b[1]) or (b[1] <= 0 < a[1]):
                t = -a[1] / (b[1] - a[1])
                x = a[0] + (b[0] - a[0]) * t
                if x < self.saddle_x and (best is None or x < best):
                    best = x
        return best


STYLES = []


def style(fn):
    STYLES.append(fn)
    return fn


def scaled(knots, sx=1.0, sy=1.0, dx=0.0):
    """Copies of knots scaled about the origin (directions adjusted for the stretch)."""
    out = []
    for k in knots:
        def adj(a):
            if a is None:
                return None
            u = unit(a)
            return math.degrees(math.atan2(u[1] * sy, u[0] * sx))
        n = K(k.p[0] * sx + dx, k.p[1] * sy, a=adj(k.a), ao=adj(k.ao), hin=k.hin, hout=k.hout)
        out.append(n)
    return out


def top_half(L, ub_x, ub_w, w_x, w_w, lb_x, lb_w, top=0.45, shoulder=0.45, waist=0.38, lower=0.42, tail=0.45):
    """Bass half of a symmetric acoustic-style outline, from the neck end to the tail on the
    string axis: upper bout, waist and lower bout at the given positions and widths."""
    return [
        K(0, 0, -90, hout=top),
        K(ub_x, -ub_w / 2, 0, hin=shoulder, hout=waist),
        K(w_x, -w_w / 2, 0, hin=waist, hout=waist),
        K(lb_x, -lb_w / 2, 0, hin=waist, hout=lower),
        K(L, 0, 90, hin=tail),
    ]


def with_cutaway(half, cut, neck_y=28.0):
    """half mirrored to the treble side, with the treble upper bout replaced by `cut`: knots
    running from the treble waist toward the neck and ending on the neck's treble edge. The
    outline then runs under the neck back to the start."""
    treble = [k.mirrored() for k in reversed(half[1:-1])][:2]      # lower bout, waist
    return list(half) + treble + list(cut) + [K(1.5, neck_y, 180, ao=-90, hin=0.2, hout=0.3)]


def soundhole_guard(hx, r, tip=(-78, 44), end=(98, 86), outer=122, ring=13):
    """Teardrop pickguard hugging the rosette on the treble side, point toward the neck."""
    R = r + ring
    arc = [(hx + R * math.cos(math.radians(a)), R * math.sin(math.radians(a)), a) for a in (150, 115, 80, 45, 15)]
    tip_p = (hx + tip[0], tip[1])
    back = (hx + tip[0] + 22, (outer + tip[1]) / 2 + 6)

    def heading(p, q):
        return math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))

    ks = [K(tip_p[0], tip_p[1], heading(back, tip_p), ao=heading(tip_p, arc[0][:2]), h=0.3)]
    for x, y, a in arc:
        ks.append(K(x, y, a - 90, ao=(50 if a == 15 else None)))
    ks += [
        K(hx + end[0], end[1], 80, h=0.45),
        K(hx + end[0] - 22, outer - 4, 170),
        K(hx + 5, outer, 180),
        K(back[0], back[1], 225),
    ]
    return ks


def acoustic(id, family, L, W, D, sx, neck_x, half, hole, cut=None, pg=None, ctx=None, spec=None, note=""):
    outline = with_cutaway(half, cut) if cut else mirror_half(half)
    s = Style(id, family, L, W, D, sx, neck_x, outline, pickguard=pg, hole=hole, spec=spec, note=note, ctx=ctx)
    s.jack = (L - 0.5, 0)                        # endpin jack
    s.straps = ((L - 0.5, 0), (1.0, 0))          # endpin and heel (under the neck)
    return s


# ---------------------------------------------------------------------------------- electric

@style
def strat():
    # Double-cutaway offset. Bass horn tip at fret 12 of a 648 scale, neck pocket at
    # fret 15.5, treble cutaway to fret 20; lower bout widest ~43 mm behind the saddle,
    # broad flat tail. Proportions checked against a 1:1 luthier template.
    sx = 325.0
    s = Style("double_cutaway_offset", "electric", 461, 321, 44, sx, sx - 187, None,
              spec=(445, 336), ctx=dict(scale=648, frets=21, nut=42, heel=56,
                                        pickups=[(159, "sc"), (99, "sc"), (41, "sc")],
                                        bridge="trem", head="6", pg="white", wood="burst"))
    s.outline_knots = STRAT_KNOTS()
    s.pickguard_knots = [
        K(66, -20, -90),
        K(78, -32, 0),
        K(87, -48, -90),
        K(78, -72, -130),
        K(58, -92, -165),
        K(36, -96, 180, hout=0.25),
        K(40, -104, 0, hin=0.25),
        K(90, -116, 3),
        K(160, -106, 12),
        K(222, -96, 0),
        K(268, -96, 25),
        K(292, -74, 80, ao=90, hout=0),
        K(292, 42, 90, hin=0, ao=10),
        K(340, 60, 10),
        K(420, 88, 45),
        K(426, 118, 150),
        K(380, 134, 185),
        K(300, 124, 195),
        K(212, 96, 180),
        K(160, 106, 160),
        K(100, 120, 180, hout=0.3),
        K(84, 113, -60, hin=0.3),
        K(114, 97, -30),
        K(131, 62, -90),
        K(122, 30, -150),
        K(90, 20, 180),
    ]
    s.controls = [s.at(-4, 64), s.at(-46, 84), s.at(-88, 102)]
    s.selector = s.at(64, 66)
    s.jack = s.at(-52, 136)
    s.straps = ((2, -106), (459, 0))
    s.arch = [K(292, -144, 30, h=0.3), K(360, -142, 5), K(430, -104, 62)]   # forearm contour
    s.arch_closed = False
    return s


def STRAT_KNOTS():
    return [
        K(60, -26, -90, h=0.3),
        K(62, -28.5, 0, h=0.3),
        K(74, -30, 10),
        K(81.5, -44, -90),
        K(74, -68, -125),
        K(58, -84, -160),
        K(35, -89.7, 180),
        K(10, -92.8, 190),
        K(0, -106.5, -90, h=0.45),
        K(10, -122.6, -40),
        K(30, -132.5, -12),
        K(62, -136.6, 0),
        K(120, -128.4, 11),
        K(195, -112.3, 0),
        K(270, -138.1, -20),
        K(368, -160.8, 0),
        K(435, -127.3, 55),
        K(461.6, 0, 90, h=0.5),
        K(435, 130.7, 125),
        K(372, 159.2, 180),
        K(300, 146.7, 195),
        K(208, 114.0, 180),
        K(165, 125.3, 158),
        K(112, 139.6, 180),
        K(80, 133.5, 205),
        K(63, 116, -90, h=0.45),
        K(80, 102, -12),
        K(112, 91, -25),
        K(126, 60, -90),
        K(117, 32, -145),
        K(106, 28.7, 180, h=0.3),
        K(60, 26, -90, h=0.3),
    ]


@style
def les_paul():
    # Single-cutaway arched. The neck enters the top of the body at fret 16 of a 628 scale,
    # the round cutaway scoop reaches fret 21, the horn tip sits ~43 mm below the bass
    # shoulder, the waist is pinched (~185 mm) and the bridge is ~0.57 along the body.
    # Proportions checked against a 1:1 luthier template.
    sx = 251.0
    s = Style("single_cutaway_arched", "electric", 438, 334, 50, sx, sx - 172, None,
              spec=(495, 328), ctx=dict(scale=628, frets=22, nut=43, heel=56,
                                        pickups=[(152, "hb"), (38, "hb")],
                                        bridge="tom", tail="stopbar", head="3+3", pg="cream", wood="burst"))
    s.outline_knots = [
        K(0, -28, -90, h=0.3),
        K(2.5, -46, -75, h=0.3),
        K(10, -69, -55),
        K(22, -92, -38),
        K(40, -109.5, -20),
        K(72, -117.8, 0),
        K(110, -108, 22),
        K(150, -91.6, 0),
        K(210, -128.1, -40),
        K(300, -162.4, 0),
        K(390, -126.8, 47),
        K(438, 0, 90, h=0.46),
        K(390, 132.9, 133),
        K(300, 166.8, 180),
        K(210, 131.7, 220),
        K(150, 92.3, 180),
        K(110, 107.7, 158),
        K(70, 117.8, 180),
        K(50, 112.4, 225),
        K(43, 104, -90, h=0.5),
        K(55, 92.6, -20),
        K(70, 77.4, -60),
        K(76.5, 62, -90),
        K(70, 47.5, -130),
        K(55, 35, -160),
        K(40, 30.8, 180, h=0.3),
        K(0, 28, -90, h=0.3),
    ]
    s.pickguard_knots = [
        K(84, 37, 180, ao=95, hin=0.3, hout=0.3),
        K(88, 62, 75),
        K(108, 82, 15),
        K(152, 86, -4),
        K(195, 70, -40),
        K(212, 47, -85, h=0.35),
        K(202, 37, 180, h=0.35),
        K(150, 37, 180),
    ]
    s.controls = [s.at(-24, 72), s.at(-44, 114), s.at(-70, 74), s.at(-90, 116)]
    s.selector = s.at(195, -90)
    s.jack = (352, 157)
    s.straps = ((28, -100), (437, 0))
    s.arch = 18.0
    return s


@style
def tele():
    # Slab single-cutaway. Shares the Fender lower bout; neck pocket at fret 15.7 of a 648
    # scale; rounded horn tip ~18 mm below the bass bout top; cutaway meets the neck at fret 20.
    sx = 265.0
    s = Style("single_cutaway_slab", "electric", 401, 324, 44, sx, sx - 187, None,
              spec=(445, 320), ctx=dict(scale=648, frets=21, nut=42, heel=56,
                                        pickups=[(165, "sc"), (36, "tele")],
                                        bridge="tele", head="6", pg="black", wood="blonde", board="maple"))
    s.outline_knots = [
        K(3, -28, -90, h=0.3),
        K(0, -70, -92),
        K(18, -130, -50),
        K(70, -150, -4),
        K(130, -148, 5),
        K(195, -137, 0),
        K(308, -161.5, 0),
        K(401, 0, 90, h=0.52),
        K(308, 162, 180),
        K(195, 138, 180),
        K(110, 147, 180),
        K(45, 132, 220),
        K(18, 100, -90, h=0.45),
        K(30, 68, -55),
        K(50, 42, -40),
        K(62, 29, -20, ao=180, hout=0.25),
        K(3, 27, 180, ao=-90, hin=0.25, hout=0.3),
    ]
    s.pickguard_knots = [
        K(8, -24, -90),
        K(7, -66, -92),
        K(20, -112, -50),
        K(62, -128, -4),
        K(128, -126, 3),
        K(176, -108, 50),
        K(193, -76, 90, ao=90, hout=0),
        K(193, 56, 90, hin=0),
        K(160, 110, 160),
        K(110, 128, 180),
        K(52, 120, 215),
        K(28, 98, -90, h=0.45),
        K(38, 70, -55),
        K(56, 44, -40),
        K(66, 32, -35, ao=195),
        K(40, 18, 180),
    ]
    s.controls = [s.at(-18, 96), s.at(-56, 96)]
    s.selector = s.at(24, 96)
    s.jack = (330, 160)
    s.straps = ((40, -146), (400, 0))
    return s


@style
def sg():
    # Thin double-cutaway: pointed horns curling toward the neck (bass horn ~13 mm longer),
    # deep U cutaways, pinched waist, lower bout wider than the upper. Neck joins at fret
    # 19.3 of a 628 scale; bevel line 14 mm inside the edge.
    sx = 250.0
    s = Style("double_cutaway_thin", "electric", 408, 322, 34, sx, sx - 172, None,
              spec=(490, 310), ctx=dict(scale=628, frets=22, nut=43, heel=56,
                                        pickups=[(150, "hb"), (38, "hb")],
                                        bridge="tom", tail="stopbar", head="3+3", pg="black", wood="red"))
    s.outline_knots = [
        K(35, -27.5, 0, h=0.3),
        K(52, -38, -60),
        K(58.5, -56, -90),
        K(52, -76, -125),
        K(38, -85, -170),
        K(12, -87.5, 180),
        K(0.5, -91, -100, h=0.35),
        K(12, -101, -35),
        K(40, -120.4, -22),
        K(95, -133.1, 0),
        K(145, -119.1, 25),
        K(188, -108.9, 0),
        K(250, -147.7, -35),
        K(300, -161, 0),
        K(385, -110.8, 60),
        K(408, 0, 90, h=0.5),
        K(385, 110.8, 120),
        K(300, 161, 180),
        K(250, 145.1, 215),
        K(182, 108.5, 180),
        K(160, 113.4, 158),
        K(100, 130.5, 180),
        K(40, 115.3, 202),
        K(13.5, 94, -80, h=0.35),
        K(30, 88, -5),
        K(48, 79, -30),
        K(57.5, 58, -90),
        K(50, 40, -125),
        K(35, 27.5, 180, h=0.3),
        K(33, 0, -90),
    ]
    s.pickguard_knots = [
        K(62, -34, 0),
        K(200, -38, 0),
        K(226, -54, -50),
        K(252, -104, -70, ao=90, hin=0.3, hout=0.3),
        K(262, -40, 85),
        K(258, 30, 100),
        K(244, 100, 110, ao=180, hin=0.3, hout=0.3),
        K(180, 97, 180),
        K(120, 110, 180),
        K(72, 112, 200),
        K(60, 96, -80),
        K(63, 64, -95),
        K(56, 44, -120),
        K(40, 30, 180),
        K(45, 0, -90),
    ]
    s.controls = [s.at(-40, 78), s.at(-56, 118), s.at(-84, 84), s.at(-100, 122)]
    s.selector = (114, 97)
    s.jack = s.at(-104, 132)
    s.straps = ((4, -93), (407, 0))
    s.arch = 14.0
    return s


@style
def es335():
    # Semi-hollow thinline: symmetric rounded ("Venetian") horns, round-bottomed cutaways
    # meeting the neck at fret 19 of a 628 scale; f-holes on the upper bouts leaning out
    # toward the tail; the bridge sits near the middle of the body.
    sx = 242.0
    s = Style("double_cutaway_semi", "electric", 483, 406, 45, sx, sx - 172, None,
              spec=(500, 400), ctx=dict(scale=628, frets=22, nut=43, heel=56,
                                        pickups=[(150, "hb"), (40, "hb")],
                                        bridge="tom", tail="stopbar", head="3+3", pg="black", wood="red"))
    half = [
        K(30, 0, -90, h=0.3),
        K(30, -28, -90, ao=-60, hin=0.3),
        K(44, -52, -90),
        K(40, -72, -125),
        K(22, -86, -165),
        K(6, -96, -135),
        K(2, -110, -80, h=0.5),
        K(16, -132, -30),
        K(45, -150, -8),
        K(72, -154, 0),
        K(115, -142, 18),
        K(160, -126, 0),
        K(225, -168, -35),
        K(318, -203, 0),
        K(430, -152, 55),
        K(483, 0, 90, h=0.52),
    ]
    s.outline_knots = mirror_half(half)
    s.pickguard_knots = [
        K(76, 45, 180, ao=95, hin=0.3, hout=0.3),
        K(80, 62, 60),
        K(104, 76, 10),
        K(150, 80, 0),
        K(194, 70, -30),
        K(211, 54, -80, h=0.35),
        K(200, 45, 180, h=0.35),
        K(140, 45, 180),
    ]
    s.hole = ("fHoles", (s.saddle_x - 118, -104), 138, 38)
    s.controls = [s.at(-40, 84), s.at(-62, 128), s.at(-86, 88), s.at(-108, 132)]
    s.selector = (38, 122)
    s.jack = (392, 180)
    s.straps = ((4, -104), (482, 0))
    s.arch = 20.0
    return s


@style
def jazzmaster():
    # Offset contoured: long bass horn, short treble horn, waists staggered (bass waist
    # ~110 mm nearer the neck than the treble one), lower bout skewed toward the treble
    # side. Neck pocket at fret 16.9 of a 648 scale. Proportions from a photo.
    sx = 293.0
    s = Style("offset", "electric", 492, 356, 44, sx, sx - 187, None,
              spec=(445, 355), ctx=dict(scale=648, frets=21, nut=42, heel=56,
                                        pickups=[(150, "p90"), (42, "p90")],
                                        bridge="floating", tail="offset_trem", head="6", pg="tortoise", wood="sea"))
    s.outline_knots = [
        K(48, -26, -90, h=0.3),
        K(52, -28.5, 0, h=0.3),
        K(59, -34, -60),
        K(60, -46, -90),
        K(52, -62, -130),
        K(34, -76, -158),
        K(14, -86, -160),
        K(3, -100, -110),
        K(1, -121, -80, h=0.45),
        K(14, -134, -30),
        K(50, -146.6, 0),
        K(110, -136.3, 12),
        K(170, -124.1, 0),
        K(230, -141.7, -22),
        K(338, -179, 0),
        K(430, -138.4, 42),
        K(470, -71.7, 70),
        K(492, 20, 95, h=0.45),
        K(470, 147, 150),
        K(405, 175.7, 180),
        K(340, 158, 205),
        K(282, 133, 180),
        K(222, 139, 170),
        K(160, 147, 180),
        K(118, 132, 225),
        K(97, 104, -95, h=0.45),
        K(101, 78, -75),
        K(109, 52, -78),
        K(113, 28.5, -88, ao=180, hin=0.3, hout=0.3),
        K(48, 27.5, 180, ao=-90, hin=0.3, hout=0.3),
    ]
    s.pickguard_knots = [
        K(62, -30, -90),
        K(66, -46, -90),
        K(56, -66, -130),
        K(38, -82, -150),
        K(24, -96, -100, ao=0, hin=0.3, hout=0.3),
        K(120, -120, 3),
        K(196, -112, 10),
        K(212, -82, 80),
        K(262, -62, 15),
        K(290, -48, 60, ao=90, hout=0),
        K(290, 64, 90, hin=0),
        K(330, 92, 25),
        K(392, 120, 55),
        K(378, 148, 170),
        K(300, 130, 190),
        K(250, 124, 175),
        K(160, 133, 180),
        K(124, 122, 225),
        K(110, 104, -90),
        K(114, 78, -75),
        K(121, 52, -78),
        K(126, 30, -90),
        K(100, 18, 180),
    ]
    s.controls = [(258, 76), (304, 84)]
    s.selector = (150, 104)
    s.jack = (356, 118)
    s.straps = ((3, -121), (490, 20))
    s.arch = [K(300, -170, 25, h=0.3), K(370, -168, 5), K(450, -120, 55)]    # forearm contour
    s.arch_closed = False
    return s


@style
def explorer():
    # Angular, straight panels: the long pointed wing is on the treble side and points at
    # the headstock; the bass edge runs from a small shoulder in a straight line to the far
    # tail corner; a notch splits the treble wing from the tail. From a front photo, scaled
    # so the nut-to-saddle distance matches a 628 scale.
    f = 1.02
    raw = [
        (80, -28), (86, -36), (92, -46), (99, -58), (106, -72), (113, -82), (121, -87.5), (130, -89.4),
        (175, -85.5), (215, -80), (224, -79.2), (232, -80.6),
        (510, -231.5), (521, -231.5), (530, -228), (535, -218), (533, -205),
        (528, -185), (520, -158.4), (510, -128.7), (500, -103.3), (480, -55.1), (460, -11.9), (440, 27.4),
        (420, 62.8), (400, 96.3), (390, 111.8), (380, 126.4), (372, 134), (366, 138), (360, 138.8), (352, 136),
        (252, 102.5), (238, 98.5), (228, 98.4), (218, 99.6),
        (100, 140.7), (40, 161.5), (26, 166), (17, 167.6), (9, 166), (3, 160), (0, 150), (1.5, 141),
        (10, 129.5), (40, 101), (60, 82.5), (76, 64), (84, 50), (86, 38), (84, 29.5), (80, 28),
    ]
    sx = 277.0
    s = Style("angular", "electric", 546, 408, 44, sx, sx - 172, [K(x * f, y * f) for x, y in raw], angular=True,
              spec=(550, 400), ctx=dict(scale=628, frets=22, nut=43, heel=56,
                                        pickups=[(150, "hb"), (40, "hb")],
                                        bridge="tom", tail="stopbar", head="6", pg="black", wood="natural"))
    pg = [(100, 34), (282, 34), (287, 42), (238, 92), (200, 95), (150, 112), (100, 130), (50, 148),
          (26, 154), (16, 150), (18, 138), (40, 110), (60, 90), (78, 70), (92, 48), (96, 38)]
    s.pickguard_knots = [K(x * f, y * f) for x, y in pg]
    s.controls = [(298, 83), (345, 68), (390, 53)]
    s.selector = (68, 130)
    s.jack = (330, 130)
    s.straps = ((124, -87), (469, -11))
    return s


@style
def archtop():
    # Full hollow archtop, single Venetian cutaway. Neck joins at fret 14 of a 628 scale;
    # floating bridge between the f-hole notches; proportions from a front photo of a
    # 16" archtop, scaled to the spec's 430 x 530.
    L, sx = 530.0, 284.0
    half = top_half(L, 92, 316, 182, 260, 353, 430, top=0.52, shoulder=0.45, waist=0.40, lower=0.44, tail=0.46)
    cut = [
        K(52, 150, 200),
        K(17, 100, -90, h=0.5),
        K(42, 92, -12),
        K(66, 80, -35),
        K(77, 58, -90),
        K(72, 36, -125),
        K(64, 28.5, 180, h=0.3),
    ]
    s = acoustic("archtop", "electric", L, 430, 82, sx, sx - 192, half,
                 ("fHoles", (sx + 4, -110), 165, 42), cut=cut,
                 spec=(530, 430),
                 ctx=dict(scale=628, frets=20, nut=43, heel=56, pickups=[(170, "hb")], bridge="floating",
                          tail="trapeze", head="3+3", pg="tortoise", wood="burst"))
    s.pickguard_knots = [
        K(84, 40, 180, ao=95, hin=0.3, hout=0.3),
        K(86, 70, 80),
        K(104, 112, 30),
        K(160, 126, 0),
        K(222, 114, -25),
        K(252, 84, -70),
        K(248, 50, -120, h=0.3),
        K(226, 40, 180, h=0.3),
        K(150, 40, 180),
    ]
    s.controls = [s.at(-56, 122), s.at(-92, 142)]
    s.jack = (430, 196)
    s.arch = 24.0
    return s


@style
def superstrat():
    # Modern superstrat: Strat-like body with sharper, longer horns and deeper cutaways
    # (treble side reaches fret 24 of a 648 scale), sculpted heel; 24-fret neck.
    sx = 322.0
    s = Style("superstrat", "electric", 448, 330, 42, sx, sx - 157, None,
              spec=(445, 336), ctx=dict(scale=648, frets=24, nut=43, heel=57,
                                        pickups=[(150, "hb"), (99, "sc"), (38, "hb")],
                                        bridge="floyd", head="6", pg=None, wood="blue", board="rosewood"))
    s.outline_knots = [
        K(70, -27.5, -90, h=0.3),
        K(80, -29, 0, h=0.3),
        K(96, -38, 30),
        K(102, -56, -90),
        K(90, -76, -140),
        K(60, -88, -172),
        K(20, -93, 182),
        K(0, -103, -110, ao=-60, hin=0.25, hout=0.3),
        K(18, -122, -25),
        K(60, -134, -2),
        K(120, -127, 12),
        K(192, -111, 0),
        K(268, -137, -22),
        K(362, -163, 0),
        K(430, -128, 55),
        K(448, 0, 90, h=0.5),
        K(430, 130, 125),
        K(366, 163, 180),
        K(296, 148, 195),
        K(206, 115, 180),
        K(160, 125, 160),
        K(100, 136, 185),
        K(52, 120, 215),
        K(36, 104, -120, ao=-20, hin=0.3, hout=0.25),
        K(90, 92, -8),
        K(140, 80, -30),
        K(160, 52, -95),
        K(150, 31, -160),
        K(130, 28.5, 180, h=0.3),
        K(70, 27, -90, h=0.3),
    ]
    s.controls = [s.at(-12, 78), s.at(-52, 100)]
    s.selector = s.at(64, 82)
    s.jack = s.at(-72, 132)
    s.straps = ((3, -103), (447, 0))
    s.arch = [K(292, -144, 30, h=0.3), K(356, -148, 5), K(424, -110, 62)]
    s.arch_closed = False
    return s


@style
def multiscale():
    # Extended-range multi-scale: the superstrat body a touch longer in the lower bout
    # (fanned frets are drawn by the renderer from the neck part).
    base = superstrat()
    s = Style("multiscale", "electric", 458, 334, 44, base.saddle_x + 6, base.neck_x, None,
              spec=(458, 336), ctx=dict(base.ctx, scale=673, nut=55, heel=68, strings=7))
    s.outline_knots = scaled(base.outline_knots, 1.022, 1.012)
    s.controls = [(p[0] * 1.022, p[1]) for p in base.controls]
    s.selector = (base.selector[0] * 1.022, base.selector[1])
    s.jack = (base.jack[0] * 1.022, base.jack[1])
    s.straps = ((3, -104), (457, 0))
    s.arch = scaled(base.arch, 1.022, 1.012)
    s.arch_closed = False
    s.note = "fanned frets drawn from the neck part"
    return s


@style
def flying_v():
    # Flying-V: two straight-edged wings, 380 mm across the wing tips, 500 mm from the neck
    # join to the wing ends; notch apex ~0.64 along; neck joins near fret 20 of a 628 scale.
    raw_half = [(0, 0), (0, -37), (2.5, -44), (8, -50), (16, -54), (26, -57),
                (486, -190), (492, -189), (495, -184), (493, -172),
                (338, -24), (331, -14), (327, -6)]
    pts = raw_half[1:] + [(x, -y) for x, y in reversed(raw_half[1:])]
    sx = 200.0
    s = Style("flying_v", "electric", 495, 380, 44, sx, sx - 172, [K(x, y) for x, y in pts], angular=True,
              spec=(500, 380), ctx=dict(scale=628, frets=22, nut=43, heel=56,
                                        pickups=[(150, "hb"), (40, "hb")],
                                        bridge="tom", tail="stopbar", head="3+3", pg="white", wood="natural"))
    pg = [(14, -44), (130, -84), (150, -66), (150, -40), (232, -40), (240, -30), (240, 30), (262, 58),
          (300, 110), (286, 118), (140, 80), (14, 44)]
    s.pickguard_knots = [K(x, y) for x, y in pg]
    s.controls = [(262, 88), (292, 116), (314, 90)]
    s.selector = (116, 72)
    s.jack = (430, 150)
    s.straps = ((8, -48), (486, 184))
    return s


def FIREBIRD_KNOTS():
    return [
        K(128, -27.5, -90, h=0.3),
        K(122, -44, -115),
        K(106, -62, -140),
        K(97, -80, -85, h=0.45),
        K(122, -98, -12),
        K(200, -110, -2),
        K(275, -114, 0),
        K(365, -140, -18),
        K(450, -166, -4),
        K(490, -148, 72, h=0.45),
        K(482, -40, 98),
        K(455, 80, 115),
        K(398, 156, 160),
        K(330, 164, 185),
        K(250, 142, 195),
        K(170, 140, 180),
        K(90, 150, 186),
        K(32, 136, 208),
        K(2, 112, -105, h=0.4),
        K(30, 96, -14),
        K(80, 78, -20),
        K(118, 54, -45),
        K(131, 33, -80),
        K(131, 28.5, -90, ao=180, hin=0.3, hout=0.2),
        K(128, 27.5, 180, ao=-90, hin=0.2, hout=0.3),
    ]


def FIREBIRD_GUARD():
    return [
        K(138, 36, 180, ao=110, hin=0.3, hout=0.3),
        K(124, 58, 135),
        K(84, 84, 160),
        K(36, 104, 170),
        K(20, 116, 110, h=0.4),
        K(60, 140, 5),
        K(150, 134, 0),
        K(230, 118, -20),
        K(276, 80, -60),
        K(282, 48, -100, h=0.3),
        K(262, 37, 180, h=0.3),
        K(200, 36, 180),
    ]


@style
def reverse_firebird():
    # Reverse body, a curvy cousin of the angular style: the treble horn is the long one and
    # reaches toward the headstock, the bass side has a short rounded shoulder at the neck,
    # a banana waist, and the tail slants so the bass side runs furthest back. Neck-through,
    # joins near fret 20.6 of a 628 scale.
    sx = 315.0
    s = Style("reverse_firebird", "electric", 492, 332, 40, sx, sx - 172, FIREBIRD_KNOTS(),
              spec=(495, 340), ctx=dict(scale=628, frets=22, nut=43, heel=56,
                                        pickups=[(150, "fb"), (40, "fb")],
                                        bridge="tom", tail="stopbar", head="6", pg="white", wood="burst"))
    s.pickguard_knots = FIREBIRD_GUARD()
    s.controls = [s.at(-50, 84), s.at(-52, 120), s.at(-86, 88), s.at(-88, 124)]
    s.selector = (165, -88)
    s.jack = s.at(-126, 118)
    s.straps = ((100, -78), (488, -130))
    return s


# ---------------------------------------------------------------------------------- bass

@style
def bass_offset():
    # J-style: offset waist (bass waist nearer the neck), long slim horns, pocket at fret 16
    # of an 864 scale; bridge ~92 mm from the tail.
    sx = 413.0
    s = Style("bass_offset", "bass", 505, 334, 44, sx, sx - 266, None,
              spec=(505, 336), ctx=dict(scale=864, frets=20, nut=38, heel=64, strings=4, saddle_spacing=19,
                                        pickups=[(160, "j"), (42, "j")],
                                        bridge="bass", head="4", pg="tortoise", wood="burst", head_len=230))
    s.outline_knots = [
        K(70, -27.5, -90, h=0.3),
        K(72, -30, 0, h=0.3),
        K(84, -34, 20),
        K(90, -50, -90),
        K(80, -74, -130),
        K(58, -88, -165),
        K(24, -93, 185),
        K(2, -104, -110, h=0.4, ao=-70),
        K(10, -122, -35),
        K(52, -140, -5),
        K(110, -130, 14),
        K(186, -108, 0),
        K(282, -146, -24),
        K(390, -168, 0),
        K(475, -120, 60),
        K(505, 8, 92, h=0.48),
        K(478, 130, 128),
        K(404, 166, 180),
        K(340, 150, 205),
        K(266, 110, 180),
        K(200, 122, 162),
        K(130, 138, 180),
        K(92, 128, 210),
        K(72, 108, -92, h=0.42),
        K(90, 94, -15),
        K(118, 84, -30),
        K(130, 56, -95),
        K(120, 31, -150),
        K(108, 28.5, 180, h=0.3),
        K(70, 27, -90, h=0.3),
    ]
    s.pickguard_knots = [
        K(76, -22, -90),
        K(88, -32, 0),
        K(96, -50, -90),
        K(84, -78, -135),
        K(62, -94, -165),
        K(44, -98, 180, hout=0.25),
        K(50, -106, 0, hin=0.25),
        K(120, -118, 8),
        K(200, -100, 5),
        K(262, -104, 10),
        K(300, -86, 60),
        K(326, -58, 80, ao=90, hout=0),
        K(326, 64, 90, hin=0),
        K(300, 102, 150),
        K(250, 104, 180),
        K(190, 112, 160),
        K(130, 124, 180),
        K(100, 118, 215),
        K(86, 106, -80),
        K(122, 92, -25),
        K(135, 58, -95),
        K(124, 30, -150),
        K(100, 18, 180),
    ]
    s.controls = [s.at(28, 92), s.at(-6, 104), s.at(-40, 114)]
    s.jack = s.at(-62, 116)
    s.straps = ((3, -104), (504, 8))
    s.arch = [K(360, -155, 25, h=0.3), K(420, -152, 5), K(482, -106, 62)]
    s.arch_closed = False
    return s


@style
def bass_p():
    # P-style: like a large double-cut electric with a symmetric waist; long bass horn,
    # split-P pickup mid-body; pocket at fret 16 of an 864 scale.
    sx = 408.0
    s = Style("bass_p", "bass", 500, 342, 44, sx, sx - 266, None,
              spec=(500, 340), ctx=dict(scale=864, frets=20, nut=42, heel=64, strings=4, saddle_spacing=19,
                                        pickups=[(170, "splitp")],
                                        bridge="bass", head="4", pg="tortoise", wood="burst", head_len=230))
    s.outline_knots = [
        K(66, -27.5, -90, h=0.3),
        K(68, -30, 0, h=0.3),
        K(80, -33, 15),
        K(87, -50, -90),
        K(78, -76, -128),
        K(58, -92, -162),
        K(28, -98, 182),
        K(1, -110, -100, h=0.45),
        K(12, -130, -35),
        K(52, -145, -4),
        K(122, -136, 12),
        K(212, -122, 0),
        K(292, -150, -22),
        K(390, -171, 0),
        K(470, -130, 58),
        K(500, 0, 90, h=0.5),
        K(470, 130, 122),
        K(390, 171, 180),
        K(292, 150, 202),
        K(212, 122, 180),
        K(150, 134, 165),
        K(100, 138, 190),
        K(72, 116, -90, h=0.45),
        K(90, 102, -15),
        K(118, 90, -30),
        K(130, 60, -92),
        K(120, 31, -150),
        K(106, 28.5, 180, h=0.3),
        K(66, 27, -90, h=0.3),
    ]
    s.pickguard_knots = [
        K(72, -22, -90),
        K(84, -32, 0),
        K(93, -50, -90),
        K(82, -80, -130),
        K(62, -98, -162),
        K(40, -104, 180, hout=0.25),
        K(46, -112, 0, hin=0.25),
        K(120, -124, 8),
        K(212, -106, 5),
        K(262, -108, 10),
        K(282, -64, 75),
        K(266, -8, 110),
        K(268, 40, 60),
        K(300, 64, 20),
        K(318, 96, 90),
        K(290, 118, 190),
        K(212, 108, 180),
        K(150, 120, 165),
        K(102, 124, 200),
        K(86, 110, -70),
        K(122, 94, -25),
        K(135, 60, -92),
        K(124, 30, -150),
        K(100, 18, 180),
    ]
    s.controls = [s.at(26, 104), s.at(-12, 118)]
    s.jack = s.at(-40, 126)
    s.straps = ((2, -110), (499, 0))
    s.arch = [K(350, -158, 25, h=0.3), K(420, -158, 5), K(478, -114, 62)]
    s.arch_closed = False
    return s


@style
def bass_musicman():
    # Music Man-style: rounder, shorter horns than the P, big round lower bout, large
    # pickguard; humbucker close to the bridge, round three-knob control plate.
    sx = 408.0
    s = Style("bass_musicman", "bass", 492, 344, 44, sx, sx - 266, None,
              spec=(500, 340), ctx=dict(scale=864, frets=21, nut=42, heel=64, strings=4, saddle_spacing=19,
                                        pickups=[(95, "mm")],
                                        bridge="bass", head="4", pg="white", wood="burst", head_len=200))
    s.outline_knots = [
        K(58, -27.5, -90, h=0.3),
        K(62, -30, 0, h=0.3),
        K(74, -36, 30),
        K(76, -56, -100),
        K(60, -78, -140),
        K(34, -90, -170),
        K(10, -96, -140),
        K(2, -114, -80, h=0.5),
        K(24, -138, -20),
        K(80, -146, 0),
        K(150, -134, 12),
        K(215, -126, 0),
        K(300, -156, -18),
        K(395, -172, 0),
        K(468, -126, 60),
        K(492, 0, 90, h=0.5),
        K(468, 126, 120),
        K(395, 172, 180),
        K(300, 156, 198),
        K(215, 126, 180),
        K(150, 134, 168),
        K(96, 138, 200),
        K(62, 110, -92, h=0.48),
        K(84, 88, -20),
        K(104, 62, -80),
        K(100, 32, -140),
        K(90, 28.5, 180, h=0.3),
        K(58, 27, -90, h=0.3),
    ]
    s.pickguard_knots = [
        K(64, -22, -90),
        K(80, -38, 30),
        K(82, -60, -100),
        K(60, -88, -150),
        K(42, -100, -90, h=0.45),
        K(70, -116, -8),
        K(120, -126, 8),
        K(230, -114, 10),
        K(300, -84, 60),
        K(310, -40, 100),
        K(292, 10, 100),
        K(296, 60, 60),
        K(322, 100, 80),
        K(290, 130, 190),
        K(200, 116, 180),
        K(140, 124, 190),
        K(96, 122, 200),
        K(78, 106, -80),
        K(104, 86, -40),
        K(112, 58, -90),
        K(100, 28, -150),
        K(80, 18, 180),
    ]
    s.controls = [s.at(-20, 104), s.at(-40, 126), s.at(-4, 128)]
    s.jack = s.at(-48, 136)
    s.straps = ((3, -114), (491, 0))
    return s


@style
def bass_thunderbird():
    # Thunderbird-style: the reverse body stretched for a bass (545 long, ~380 across the
    # points), neck-through joining near fret 19.6 of an 864 scale.
    fx, fy = 545 / 492, 1.14
    sx = 425.0
    s = Style("bass_thunderbird", "bass", 545, 380, 44, sx, sx - 266, scaled(FIREBIRD_KNOTS(), fx, fy),
              spec=(545, 380), ctx=dict(scale=864, frets=20, nut=40, heel=64, strings=4, saddle_spacing=19,
                                        pickups=[(160, "hb"), (60, "hb")],
                                        bridge="bass", head="4", pg="white", wood="burst", head_len=230))
    s.pickguard_knots = scaled(FIREBIRD_GUARD(), fx, fy)
    s.controls = [s.at(-24, 100), s.at(-46, 124), s.at(-58, 96)]
    s.jack = s.at(-62, 136)
    s.straps = ((108, -89), (541, -148))
    return s


@style
def bass_hollow():
    # Hollow-body bass: the thinline double-cut shape at bass size, f-holes on the upper
    # bouts, trapeze tailpiece. Neck joins near fret 17 of an 864 scale.
    base = es335()
    fx, fy = 525 / 483, 400 / 406
    sx = 357.0
    s = Style("bass_hollow", "bass", 525, 400, 85, sx, sx - 266, scaled(base.outline_knots, fx, fy),
              spec=(525, 400), ctx=dict(scale=864, frets=20, nut=42, heel=64, strings=4, saddle_spacing=19,
                                        pickups=[(150, "mm")],
                                        bridge="bass", tail="trapeze", head="4", pg="tortoise", wood="burst", head_len=230))
    s.hole = ("fHoles", (118 * fx, -104 * fy), 150, 40)
    s.pickguard_knots = scaled([
        K(104, 40, 180, ao=90, hin=0.3, hout=0.3),
        K(106, 72, 80),
        K(135, 102, 20),
        K(185, 98, -30),
        K(205, 66, -70),
        K(200, 42, -180, h=0.35),
        K(150, 40, 180),
    ], fx, fy)
    s.controls = [s.at(-60, 110), s.at(-96, 132)]
    s.jack = (430, 183)
    s.straps = ((4, -102), (524, 0))
    s.arch = 20.0
    return s


@style
def bass_violin():
    # Violin-shaped hollow bass: sloping shoulders, pointed violin corners either side of a
    # deep C-bout waist, round lower bout; bridge ~0.65 along. From a drawing of the classic
    # 1960s model; the body is the same whatever the scale.
    sx = 323.0
    half = [
        K(0, 0, -90, h=0.3),
        K(0, -30, -90, ao=-20, hin=0.3, hout=0.35),
        K(49, -46, -30),
        K(88, -93, -50),
        K(138, -118, -12),
        K(192, -118, 20, ao=110, hin=0.3, hout=0.25),
        K(215, -88, 60),
        K(243, -83, 0),
        K(271, -90, -40),
        K(296, -129, -110, ao=-10, hin=0.25, hout=0.3),
        K(354, -144, -3),
        K(392, -145, 0),
        K(436, -137, 18),
        K(470, -115, 40),
        K(492, -76, 72),
        K(500, 0, 90, h=0.45),
    ]
    s = Style("bass_violin", "bass", 500, 290, 80, sx, sx - 237, mirror_half(half),
              spec=(500, 290), ctx=dict(scale=864, frets=22, nut=42, heel=52, strings=4, saddle_spacing=19,
                                        pickups=[(120, "mm"), (40, "splitp")],
                                        bridge="floating", tail="trapeze", head="4", pg="cream", wood="burst", head_len=200))
    s.pickguard_knots = [
        K(52, 30, 180, ao=90, hin=0.3, hout=0.3),
        K(80, 76, 50),
        K(130, 104, 10),
        K(180, 96, -20),
        K(200, 78, 10),
        K(250, 72, 0),
        K(296, 70, -30),
        K(312, 50, -95, h=0.3),
        K(300, 30, 180, h=0.3),
        K(150, 30, 180),
    ]
    s.controls = [(374, 116), (429, 70)]
    s.selector = (401, 93)
    s.jack = (478, 96)
    s.straps = ((499.5, 0), (1, 0))
    return s


@style
def acoustic_bass():
    # Acoustic bass: a big round-shouldered flat-top; 34" scale puts the bridge low in the
    # lower bout (neck joins at fret 14).
    L = 555.0
    sx = 385.0
    half = top_half(L, 128, 322, 238, 300, 400, 435, top=0.5, shoulder=0.5, waist=0.40, lower=0.46, tail=0.5)
    hx = 172.0
    s = acoustic("acoustic_bass", "bass", L, 435, 135, sx, sx - 266, half, ("round", (hx, 0), 104, 104),
                 pg=soundhole_guard(hx, 52, tip=(-80, 46), end=(100, 90), outer=130),
                 spec=(555, 435), ctx=dict(scale=864, frets=20, nut=42, heel=60, strings=4, saddle_spacing=19,
                                           bridge="pin", head="4", pg="tortoise", wood="spruce", head_len=210))
    return s


@style
def headless_bass():
    # Headless modern bass: compact double-cut body, deep cutaways, tuners behind the bridge.
    sx = 300.0
    s = Style("headless_bass", "bass", 380, 320, 42, sx, sx - 212, None,
              spec=(380, 320), ctx=dict(scale=864, frets=24, nut=42, heel=66, strings=4, saddle_spacing=19,
                                        pickups=[(150, "j"), (42, "hb")],
                                        bridge="bass", head="none", pg=None, wood="black", head_len=30))
    s.outline_knots = [
        K(40, -28, -90, h=0.3),
        K(48, -32, 20),
        K(56, -52, -90),
        K(44, -74, -140),
        K(16, -86, -170),
        K(0, -100, -100, ao=-50, hin=0.25, hout=0.3),
        K(30, -128, -12),
        K(100, -134, 5),
        K(170, -118, 0),
        K(270, -160, 0),
        K(350, -128, 55),
        K(380, 0, 90, h=0.48),
        K(350, 124, 125),
        K(262, 156, 180),
        K(180, 118, 180),
        K(110, 132, 180),
        K(58, 118, 215),
        K(30, 100, -120, ao=-10, hin=0.3, hout=0.3),
        K(72, 84, -20),
        K(92, 58, -95),
        K(86, 31, -150),
        K(76, 28.5, 180, h=0.3),
        K(40, 27, -90, h=0.3),
    ]
    s.controls = [s.at(-6, 96), s.at(-40, 106)]
    s.jack = s.at(-40, 128)
    s.straps = ((2, -100), (379, 0))
    s.arch = [K(236, -140, 25, h=0.3), K(300, -146, 5), K(356, -112, 62)]
    s.arch_closed = False
    return s


# ---------------------------------------------------------------------------------- acoustic

@style
def dreadnought():
    # 14-fret dreadnought: square shoulders, shallow waist; neck joins at fret 14 of 645.
    L, sx, hx, hr = 505.0, 287.0, 138.0, 51.0
    half = top_half(L, 118, 292, 224, 272, 372, 398, top=0.56, shoulder=0.58, waist=0.40, lower=0.46, tail=0.50)
    return acoustic("dreadnought", "acoustic", L, 400, 122, sx, sx - 198, half, ("round", (hx, 0), 2 * hr, 2 * hr),
                    pg=soundhole_guard(hx, hr, tip=(-74, 46), end=(98, 84), outer=118),
                    spec=(505, 400), ctx=dict(scale=645, frets=20, nut=43, heel=56, bridge="pin", head="3+3",
                                              pg="tortoise", wood="spruce"))


@style
def grand_auditorium():
    # Grand auditorium: rounder shoulders and a narrower waist than the dreadnought.
    L, sx, hx, hr = 502.0, 287.0, 140.0, 50.0
    half = top_half(L, 112, 295, 218, 238, 368, 405, top=0.47, shoulder=0.45, waist=0.40, lower=0.45, tail=0.48)
    return acoustic("grand_auditorium", "acoustic", L, 405, 115, sx, sx - 198, half, ("round", (hx, 0), 2 * hr, 2 * hr),
                    pg=soundhole_guard(hx, hr, tip=(-74, 44), end=(96, 84), outer=112),
                    spec=(502, 405), ctx=dict(scale=645, frets=20, nut=43, heel=56, bridge="pin", head="3+3",
                                              pg="tortoise", wood="spruce", board="ebony"))


@style
def grand_auditorium_cutaway():
    # Grand auditorium with a Venetian cutaway (spec 4.2 "cutaway variant available").
    L, sx, hx, hr = 502.0, 287.0, 140.0, 50.0
    half = top_half(L, 112, 295, 218, 238, 368, 405, top=0.47, shoulder=0.45, waist=0.40, lower=0.45, tail=0.48)
    cut = [K(46, 142, 215), K(20, 104, -90, h=0.5), K(46, 90, -15), K(70, 74, -40), K(80, 52, -90),
           K(74, 33, -130), K(64, 28.5, 180, h=0.3)]
    return acoustic("grand_auditorium_cutaway", "acoustic", L, 405, 115, sx, sx - 198, half,
                    ("round", (hx, 0), 2 * hr, 2 * hr), cut=cut,
                    pg=soundhole_guard(hx, hr, tip=(-60, 48), end=(96, 84), outer=112),
                    spec=(502, 405), ctx=dict(scale=645, frets=20, nut=43, heel=56, bridge="pin", head="3+3",
                                              pg="tortoise", wood="spruce", board="ebony"))


@style
def om():
    # Orchestra model: smaller than a dreadnought with a GA-like waist; 14 frets clear.
    L, sx, hx, hr = 495.0, 287.0, 138.0, 49.0
    half = top_half(L, 112, 286, 212, 235, 358, 380, top=0.46, shoulder=0.44, waist=0.40, lower=0.45, tail=0.48)
    return acoustic("om", "acoustic", L, 380, 108, sx, sx - 198, half, ("round", (hx, 0), 2 * hr, 2 * hr),
                    pg=soundhole_guard(hx, hr, tip=(-72, 44), end=(94, 82), outer=110),
                    spec=(495, 380), ctx=dict(scale=645, frets=20, nut=44, heel=56, bridge="pin", head="3+3",
                                              pg="tortoise", wood="spruce"))


@style
def auditorium_000():
    # 000: the OM outline on a slightly shorter body (and shorter scale in life).
    L, sx, hx, hr = 490.0, 282.0, 136.0, 49.0
    half = top_half(L, 110, 286, 210, 235, 355, 380, top=0.46, shoulder=0.44, waist=0.40, lower=0.45, tail=0.48)
    return acoustic("auditorium_000", "acoustic", L, 380, 105, sx, sx - 194, half, ("round", (hx, 0), 2 * hr, 2 * hr),
                    pg=soundhole_guard(hx, hr, tip=(-72, 44), end=(94, 82), outer=110),
                    spec=(490, 380), ctx=dict(scale=632, frets=20, nut=44, heel=56, bridge="pin", head="3+3",
                                              pg="tortoise", wood="spruce"))


@style
def parlor():
    # 12-fret parlor: small, narrow body; the 12-fret join puts the bridge low in the lower
    # bout and the soundhole well down from the top.
    L, sx, hx, hr = 460.0, 322.0, 168.0, 44.0
    half = top_half(L, 98, 242, 190, 212, 328, 330, top=0.44, shoulder=0.44, waist=0.40, lower=0.46, tail=0.5)
    return acoustic("parlor", "acoustic", L, 330, 95, sx, sx - 198, half, ("round", (hx, 0), 2 * hr, 2 * hr),
                    spec=(460, 330), ctx=dict(scale=645, frets=20, nut=43, heel=54, bridge="pin", head="3+3",
                                              pg=None, wood="cedar"), note="12 frets clear of the body")


@style
def jumbo():
    # Jumbo: big round lower bout, pinched waist, round upper bout (the classic super-jumbo
    # look); 14 frets clear.
    L, sx, hx, hr = 520.0, 287.0, 138.0, 51.0
    half = top_half(L, 100, 302, 208, 240, 372, 425, top=0.40, shoulder=0.40, waist=0.44, lower=0.50, tail=0.56)
    return acoustic("jumbo", "acoustic", L, 425, 122, sx, sx - 198, half, ("round", (hx, 0), 2 * hr, 2 * hr),
                    pg=soundhole_guard(hx, hr, tip=(-76, 46), end=(112, 92), outer=118, ring=14),
                    spec=(520, 425), ctx=dict(scale=645, frets=20, nut=43, heel=56, bridge="pin", head="3+3",
                                              pg="tortoise", wood="burst"))


@style
def gypsy_jazz():
    # Selmer-style petite bouche: broad waist, cutaway on the treble side, small oval hole
    # (long axis along the strings) under the fretboard end, floating moustache bridge low
    # in the lower bout; 14 frets clear.
    L, sx, hx = 505.0, 297.0, 150.0
    half = top_half(L, 104, 322, 214, 302, 364, 400, top=0.5, shoulder=0.46, waist=0.40, lower=0.45, tail=0.5)
    cut = [K(40, 148, 212), K(12, 118, -110, h=0.45), K(26, 92, -40), K(60, 70, -35), K(80, 48, -80),
           K(76, 32, -130), K(66, 28.5, 180, h=0.3)]
    s = acoustic("gypsy_jazz", "acoustic", L, 400, 100, sx, sx - 203, half, ("oval", (hx, 0), 110, 72), cut=cut,
                 pg=[K(hx + 12, 64, 40), K(hx + 70, 92, 20), K(hx + 92, 118, 90, h=0.45), K(hx + 50, 128, 185),
                     K(hx - 10, 96, 230)],
                 spec=(505, 400), ctx=dict(scale=670, frets=21, nut=44, heel=56, bridge="moustache", tail="trapeze",
                                           head="3+3", pg="tortoise", wood="spruce", board="ebony"))
    return s


@style
def gypsy_jazz_grande_bouche():
    # Grande bouche: the same body with a large D-shaped hole (flat side toward the bridge)
    # and a 12-fret join, so the bridge sits further down.
    L, sx, hx = 505.0, 335.0, 148.0
    half = top_half(L, 104, 322, 214, 302, 364, 400, top=0.5, shoulder=0.46, waist=0.40, lower=0.45, tail=0.5)
    cut = [K(40, 148, 212), K(12, 118, -110, h=0.45), K(26, 92, -40), K(60, 70, -35), K(80, 48, -80),
           K(76, 32, -130), K(66, 28.5, 180, h=0.3)]
    s = acoustic("gypsy_jazz_grande_bouche", "acoustic", L, 400, 100, sx, sx - 210, half,
                 ("dShape", (hx, 0), 100, 146), cut=cut,
                 spec=(505, 400), ctx=dict(scale=670, frets=21, nut=44, heel=56, bridge="moustache", tail="trapeze",
                                           head="3+3", pg=None, wood="spruce", board="ebony"),
                 note="12 frets clear of the body")
    return s


# ---------------------------------------------------------------------------------- classical

def classical_half(L=490.0):
    return top_half(L, 104, 280, 206, 236, 352, 370, top=0.46, shoulder=0.44, waist=0.40, lower=0.45, tail=0.48)


@style
def classical():
    # Classical: 12-fret join (650 scale), round soundhole under the fingerboard end, wide
    # flat neck, tie-block bridge low in the lower bout.
    L, sx, hx, hr = 490.0, 325.0, 152.0, 43.0
    return acoustic("classical", "classical", L, 370, 100, sx, sx - 211, classical_half(L),
                    ("round", (hx, 0), 2 * hr, 2 * hr),
                    spec=(490, 370), ctx=dict(scale=650, frets=19, nut=52, heel=60, bridge="tie", head="slotted",
                                              pg=None, wood="cedar"))


@style
def flamenco():
    # Flamenca: the classical outline on a thinner body, clear golpeador plates below and
    # around the soundhole.
    L, sx, hx, hr = 490.0, 325.0, 152.0, 43.0
    R = hr + 14
    pg = [
        K(hx - 46, -104, -95, ao=5, h=0.3),
        K(hx + 40, -112, 5),
        K(hx + 112, -96, 40),
        K(hx + 128, 0, 90),
        K(hx + 112, 96, 140),
        K(hx + 40, 112, 175),
        K(hx - 46, 104, 185, ao=-85, h=0.3),
    ]
    for a in (140, 100, 60, 20, -20, -60, -100, -140):
        pg.append(K(hx + R * math.cos(math.radians(a)), R * math.sin(math.radians(a)), a - 90))
    return acoustic("flamenco", "classical", L, 370, 92, sx, sx - 211, classical_half(L),
                    ("round", (hx, 0), 2 * hr, 2 * hr), pg=pg,
                    spec=(490, 370), ctx=dict(scale=650, frets=19, nut=52, heel=60, bridge="tie", head="slotted",
                                              pg="clear", wood="spruce"), note="golpeador as the pickguard")


@style
def cutaway_classical():
    # Classical with a Venetian cutaway on the treble side.
    L, sx, hx, hr = 490.0, 325.0, 152.0, 43.0
    cut = [K(46, 132, 215), K(22, 98, -90, h=0.5), K(48, 84, -15), K(76, 70, -40), K(88, 50, -90),
           K(82, 32, -130), K(72, 28.5, 180, h=0.3)]
    return acoustic("cutaway_classical", "classical", L, 370, 100, sx, sx - 211, classical_half(L),
                    ("round", (hx, 0), 2 * hr, 2 * hr), cut=cut,
                    spec=(490, 370), ctx=dict(scale=650, frets=19, nut=52, heel=60, bridge="tie", head="slotted",
                                              pg=None, wood="cedar"))


# ---------------------------------------------------------------------------------- resonator

def reso_half(L=505.0):
    return top_half(L, 84, 300, 188, 282, 350, 385, top=0.58, shoulder=0.56, waist=0.40, lower=0.46, tail=0.5)


@style
def resonator():
    # Round-neck steel resonator: squarish upper bout with two f-holes, shallow waist, big
    # lower bout carrying the cover plate; 12-fret join, biscuit bridge on the cone centre.
    L, sx = 505.0, 318.0
    s = acoustic("resonator", "resonator", L, 385, 90, sx, sx - 206, reso_half(L),
                 ("resonatorCover", (sx, 0), 250, 250),
                 spec=(505, 385), ctx=dict(scale=635, frets=19, nut=44, heel=56, bridge="biscuit", tail="trapeze",
                                           head="3+3", pg=None, wood="metal"), note="12 frets clear of the body")
    s.upper_holes = ("fHoles", (88, -86), 112, 30)
    return s


@style
def resonator_square_neck():
    # Square-neck (lap) resonator: the same body with two round screened upper-bout holes
    # and a larger cover plate over a spider-bridge cone.
    L, sx = 505.0, 318.0
    s = acoustic("resonator_square_neck", "resonator", L, 385, 90, sx, sx - 206, reso_half(L),
                 ("resonatorCover", (sx, 0), 262, 262),
                 spec=(505, 385), ctx=dict(scale=635, frets=19, nut=52, heel=60, bridge="spider", tail="trapeze",
                                           head="3+3", pg=None, wood="natural"), note="12 frets clear of the body")
    s.upper_holes = ("round", (92, -84), 48, 48)
    return s


# ----------------------------------------------------------------------------------------------
# Header

def fmt(v):
    s = "%.4f" % v
    if s == "-0.0000":
        s = "0.0000"
    return s + "f"


def pt(style_, p, corner=False):
    u, v = style_.uv(p)
    return "{ %s, %s%s }" % (fmt(u), fmt(v), ", true" if corner else "")


def pt_list(style_, pts, per_line=4, corners=None):
    corners = corners or [False] * len(pts)
    items = [pt(style_, p, c) for p, c in zip(pts, corners)]
    lines = []
    for i in range(0, len(items), per_line):
        lines.append("    " + ", ".join(items[i:i + per_line]))
    return ",\n".join(lines)


HEADER_TOP = """#pragma once
// Generated by Tools/body_outlines.py - do not edit by hand.
//
// Body outlines and body-anchored details for every body style
// (spec/guitar-illustration.md 4, 5 and 9).
//
// Coordinates are normalised per style:
//   u along the guitar axis: 0 = the body's leftmost point (horn tips / upper bout
//     nearest the headstock), 1 = the tail. Real length = lengthMm.
//   v lateral: negative = bass side (top of the screen), positive = treble. The strings
//     run along v = 0. widthMm is twice the largest distance of the outline from the
//     string axis, so max |v| is 0.5 on both symmetric and offset bodies; physWidthMm is
//     the body's actual width.
// Scene millimetres (section 1): X = (saddleU - u) * lengthMm (toward the headstock),
// Y = v * widthMm (toward the treble side).
//
// Drawing:
//   outline, pickguard and arch are drawn through their points with a centripetal
//   Catmull-Rom spline (catmullRomToBezier below, applied to the points after they are
//   mapped to millimetres) unless the style is angular, when they are straight segments.
//   A point with corner = true is turned sharply: segments either side of it use the
//   corner itself as their far neighbour (P0 := P1 when P1 is a corner, P3 := P2 when
//   P2 is a corner).
//   outline and pickguard are closed. arch is closed when archClosed, else an open line.
//   Pickguards may run under the neck; draw the neck over them.
//   Holes: holeWidthMm is the extent along the guitar axis, holeHeightMm across it.
//     round / oval / dShape are centred on holeCentre (a dShape's flat side faces the
//     bridge). fHoles: holeCentre is the bass-side f-hole; mirror it to the treble side
//     (v -> -v); f-holes lean so their neck ends sit nearer the strings.
//     resonatorCover: the cover plate, holeWidthMm across.
//   upperHoles: a second pair of holes (resonator upper-bout sound holes), bass-side
//     centre given, mirrored like f-holes.
//   neckU: where the neck and fretboard end on the body for the style's usual neck.

#include <cmath>
#include <cstddef>
#include <cstring>

namespace luthier::outlines
{

struct Pt { float u, v; bool corner = false; };

enum class Hole { none, round, oval, dShape, fHoles, resonatorCover };

struct BodyStyle
{
    const char* id;                 // matches a guitar file's meta.body_style
    const char* family;             // electric / acoustic / classical / bass / resonator
    float lengthMm, widthMm, depthMm;
    float saddleU, neckU;
    bool angular;                   // true: straight segments; false: Catmull-Rom smoothed
    const Pt* outline; int numOutline;
    const Pt* pickguard; int numPickguard;
    Hole hole; Pt holeCentre; float holeWidthMm, holeHeightMm;
    const Pt* controls; int numControls;
    Pt selector; bool hasSelector;
    Pt jack;
    Pt strapButtons[2];
    const Pt* arch; int numArch;
    bool archClosed;
    Hole upperHoles; Pt upperHoleCentre; float upperHoleWidthMm, upperHoleHeightMm;
    float physWidthMm;
};

/** Centripetal Catmull-Rom (alpha 0.5) segment p1 -> p2 as a cubic Bezier: c1 and c2 are
    the control points. For a closed outline of n points, segment i uses points
    i-1, i, i+1, i+2 (indices wrapped). For an open line, extend the ends by reflection
    (p[-1] = 2 p[0] - p[1]). Apply it in millimetres, not in normalised u / v. */
inline void catmullRomToBezier (float x0, float y0, float x1, float y1, float x2, float y2,
                                float x3, float y3, float& c1x, float& c1y, float& c2x, float& c2y) noexcept
{
    const auto len = [] (float ax, float ay, float bx, float by)
    {
        const float d = std::sqrt (std::sqrt ((bx - ax) * (bx - ax) + (by - ay) * (by - ay)));
        return d > 1.0e-4f ? d : 1.0e-4f;
    };
    const float d1 = len (x0, y0, x1, y1), d2 = len (x1, y1, x2, y2), d3 = len (x2, y2, x3, y3);
    const float a1 = 2 * d1 * d1 + 3 * d1 * d2 + d2 * d2, n1 = 3 * d1 * (d1 + d2);
    const float a2 = 2 * d3 * d3 + 3 * d3 * d2 + d2 * d2, n2 = 3 * d3 * (d3 + d2);
    c1x = (d1 * d1 * x2 - d2 * d2 * x0 + a1 * x1) / n1;
    c1y = (d1 * d1 * y2 - d2 * d2 * y0 + a1 * y1) / n1;
    c2x = (d3 * d3 * x1 - d2 * d2 * x3 + a2 * x2) / n2;
    c2y = (d3 * d3 * y1 - d2 * d2 * y3 + a2 * y2) / n2;
}
"""


def describe(fn):
    """The comment block at the top of a style function, for the header."""
    import inspect
    lines = []
    for line in inspect.getsource(fn).splitlines()[2:]:
        line = line.strip()
        if not line.startswith("#"):
            break
        lines.append(line.lstrip("#").strip())
    return lines


def emit_header(styles):
    out = [HEADER_TOP]
    for s in styles:
        c = s.id
        out.append("\n// %s - %s%s\n" % (c, s.family, (" (" + s.note + ")") if s.note else ""))
        for line in s.doc:
            out.append("//   %s\n" % line)
        out.append("inline constexpr Pt %s_outline[] = {\n%s\n};\n" % (c, pt_list(s, s.outline, corners=s.outline_corners)))
        if s.pickguard:
            out.append("inline constexpr Pt %s_pickguard[] = {\n%s\n};\n"
                       % (c, pt_list(s, s.pickguard, corners=s.pickguard_corners)))
        if s.controls:
            out.append("inline constexpr Pt %s_controls[] = {\n%s\n};\n" % (c, pt_list(s, s.controls)))
        if s.arch_pts:
            out.append("inline constexpr Pt %s_arch[] = {\n%s\n};\n" % (c, pt_list(s, s.arch_pts, corners=s.arch_corners)))

    out.append("\ninline constexpr BodyStyle kBodyStyles[] = {\n")
    zero = "{ 0.0f, 0.0f }"
    for s in styles:
        c = s.id
        hole, hc, hw, hh = s.hole
        uh, uc, uw, uhh = s.upper_holes
        rows = [
            ('"%s", "%s",' % (c, s.family), ""),
            ("%s, %s, %s," % (fmt(s.length), fmt(s.width), fmt(s.depth)), "length, width, depth (mm)"),
            ("%s, %s, %s," % (fmt(s.uv((s.saddle_x, 0))[0]), fmt(s.uv((s.neck_x, 0))[0]),
                              "true" if s.angular else "false"), "saddleU, neckU, angular"),
            ("%s_outline, %d," % (c, len(s.outline)), "outline"),
            (("%s_pickguard, %d," % (c, len(s.pickguard))) if s.pickguard else "nullptr, 0,", "pickguard"),
            ("Hole::%s, %s, %s, %s," % (hole, pt(s, hc) if hole != "none" else zero, fmt(hw), fmt(hh)),
             "hole, centre, along, across (mm)"),
            (("%s_controls, %d," % (c, len(s.controls))) if s.controls else "nullptr, 0,", "controls"),
            ("%s, %s," % (pt(s, s.selector) if s.selector else zero, "true" if s.selector else "false"), "selector"),
            ("%s," % pt(s, s.jack), "jack"),
            ("{ %s, %s }," % (pt(s, s.straps[0]), pt(s, s.straps[1])), "strap buttons"),
            (("%s_arch, %d, %s," % (c, len(s.arch_pts), "true" if s.arch_closed else "false"))
             if s.arch_pts else "nullptr, 0, false,", "arch, closed"),
            ("Hole::%s, %s, %s, %s," % (uh, pt(s, uc) if uh != "none" else zero, fmt(uw), fmt(uhh)), "upper holes"),
            ("%s }," % fmt(s.phys_width), "physWidthMm"),
        ]
        width = max(len(r[0]) for r in rows) + 2
        for i, (code, comment) in enumerate(rows):
            lead = "    { " if i == 0 else "      "
            out.append((lead + code.ljust(width) + ("// " + comment if comment else "")).rstrip() + "\n")
    out.append("};\n\ninline constexpr int kNumBodyStyles = (int) (sizeof (kBodyStyles) / sizeof (kBodyStyles[0]));\n")
    out.append("""
/** The style with this id, or nullptr if there is none. */
inline const BodyStyle* findBodyStyle (const char* id) noexcept
{
    if (id == nullptr)
        return nullptr;

    for (int i = 0; i < kNumBodyStyles; ++i)
        if (std::strcmp (kBodyStyles[i].id, id) == 0)
            return &kBodyStyles[i];

    return nullptr;
}

} // namespace luthier::outlines
""")
    text = "".join(out)
    os.makedirs(os.path.dirname(HEADER), exist_ok=True)
    with open(HEADER, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(text)


# ----------------------------------------------------------------------------------------------
# Preview

WOODS = {
    "burst": ((74, 36, 18), (214, 150, 62)),
    "blonde": ((196, 160, 96), (232, 206, 150)),
    "spruce": ((196, 150, 86), (232, 196, 128)),
    "cedar": ((150, 92, 50), (196, 128, 72)),
    "mahogany": ((96, 48, 26), (140, 72, 38)),
    "red": ((92, 16, 12), (150, 30, 22)),
    "black": ((20, 20, 20), (44, 44, 44)),
    "metal": ((150, 154, 160), (206, 210, 214)),
    "natural": ((176, 128, 70), (214, 170, 104)),
    "sea": ((96, 170, 150), (150, 206, 190)),
    "blue": ((20, 44, 110), (40, 76, 160)),
}
PG = {"white": (238, 234, 222), "black": (24, 22, 22), "tortoise": (92, 40, 20), "cream": (226, 208, 160),
      "clear": (220, 230, 235, 70), "mint": (206, 222, 196)}
BG = (30, 23, 19)


def font(size, bold=False):
    from PIL import ImageFont
    for name in (("arialbd.ttf" if bold else "arial.ttf"), "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


class View:
    def __init__(self, draw, ox, oy, s, ss):
        self.d, self.ox, self.oy, self.s, self.ss = draw, ox, oy, s, ss

    def P(self, p):
        return ((self.ox + p[0] * self.s) * self.ss, (self.oy + p[1] * self.s) * self.ss)

    def poly(self, pts, fill=None, outline=None, width=1.0):
        q = [self.P(p) for p in pts]
        if fill is not None:
            self.d.polygon(q, fill=fill)
        if outline is not None:
            self.d.line(q + [q[0]], fill=outline, width=max(1, int(round(width * self.ss))), joint="curve")

    def line(self, pts, fill, width=1.0):
        self.d.line([self.P(p) for p in pts], fill=fill, width=max(1, int(round(width * self.ss))), joint="curve")

    def circle(self, c, r, fill=None, outline=None, width=1.0):
        x, y = self.P(c)
        rr = r * self.s * self.ss
        self.d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=fill, outline=outline,
                       width=max(1, int(round(width * self.ss))))

    def ellipse(self, c, rx, ry, fill=None, outline=None, width=1.0):
        x, y = self.P(c)
        a, b = rx * self.s * self.ss, ry * self.s * self.ss
        self.d.ellipse([x - a, y - b, x + a, y + b], fill=fill, outline=outline,
                       width=max(1, int(round(width * self.ss))))

    def rect(self, x0, y0, x1, y1, fill=None, outline=None, width=1.0):
        self.poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], fill, outline, width)

    def text(self, p, s, size, fill=(239, 227, 204), bold=False):
        x, y = self.P(p)
        self.d.text((x, y), s, font=font(int(size * self.ss), bold), fill=fill)


def rot(p, c, deg):
    r = math.radians(deg)
    x, y = p[0] - c[0], p[1] - c[1]
    return (c[0] + x * math.cos(r) - y * math.sin(r), c[1] + x * math.sin(r) + y * math.cos(r))


def f_hole(cx, cy, length, width, lean):
    """A simple f-hole: a slim lens with round eyes, leaning by `lean` degrees."""
    pts = []
    n = 24
    for i in range(n + 1):
        t = i / n
        x = cx - length / 2 + length * t
        w = width * 0.22 * math.sin(math.pi * t) + 1.2
        y = cy + width * 0.28 * math.sin(2 * math.pi * t)
        pts.append((x, y - w))
    for i in range(n, -1, -1):
        t = i / n
        x = cx - length / 2 + length * t
        w = width * 0.22 * math.sin(math.pi * t) + 1.2
        y = cy + width * 0.28 * math.sin(2 * math.pi * t)
        pts.append((x, y + w))
    pts = [rot(p, (cx, cy), lean) for p in pts]
    eyes = [rot((cx - length / 2 + 4, cy - width * 0.05), (cx, cy), lean),
            rot((cx + length / 2 - 4, cy + width * 0.05), (cx, cy), lean)]
    return pts, eyes, width * 0.2


def draw_guitar(img, s, ox, oy, scale_px, ss, label=True):
    from PIL import Image, ImageDraw, ImageFilter
    ctx = s.ctx
    fam = s.family
    v = View(ImageDraw.Draw(img, "RGBA"), ox, oy, scale_px, ss)
    scale = ctx.get("scale", 648.0)
    sx = s.saddle_x
    nut_x = sx - scale
    body = s.outline_smooth

    # shadow
    sh = Image.new("L", img.size, 0)
    dv = View(ImageDraw.Draw(sh), ox + 5, oy + 7, scale_px, ss)
    dv.poly(body, fill=150)
    sh = sh.filter(ImageFilter.GaussianBlur(7 * ss))
    img.paste((0, 0, 0), (0, 0), sh)
    v = View(ImageDraw.Draw(img, "RGBA"), ox, oy, scale_px, ss)

    edge, centre = WOODS.get(ctx.get("wood", "natural"), WOODS["natural"])
    if fam in ("acoustic", "classical", "resonator") or s.id in ("archtop", "double_cutaway_semi", "bass_hollow", "bass_violin"):
        v.poly(body, fill=edge)
        inner = offset_ring(body[:-1], 7)
        v.poly(inner, fill=centre)
        v.poly(inner, outline=(246, 236, 210), width=1.2)
    else:
        v.poly(body, fill=edge)
        try:
            inner = offset_ring(body[:-1], 30)
            v.poly(inner, fill=tuple((a + b) // 2 for a, b in zip(edge, centre)))
            inner2 = offset_ring(body[:-1], 60)
            v.poly(inner2, fill=centre)
        except Exception:
            pass
    v.poly(body, outline=(12, 8, 6), width=1.4)

    if s.arch_pts:
        a = smooth(s.arch_pts, s.arch_closed, s.angular, corners=s.arch_corners)
        if s.arch_closed:
            v.poly(a, outline=(255, 240, 200, 90), width=2.0)
        else:
            v.line(a, fill=(255, 240, 200, 110), width=3.0)

    # pickguard
    if s.pickguard:
        pg = smooth(s.pickguard, True, s.angular, corners=s.pickguard_corners)
        col = PG.get(ctx.get("pg", "black"), (24, 22, 22))
        v.poly(pg, fill=col if len(col) == 4 else col + (255,))
        v.poly(pg, outline=(0, 0, 0, 160), width=1.0)

    # holes
    kind, hc, hw, hh = s.hole
    if kind == "round":
        r = hw / 2
        v.circle(hc, r + 14, fill=(60, 36, 20))
        v.circle(hc, r + 11, fill=(230, 214, 176))
        v.circle(hc, r + 7, fill=(40, 26, 16))
        v.circle(hc, r + 4, fill=(230, 214, 176))
        v.circle(hc, r, fill=(16, 10, 8))
    elif kind == "oval":
        v.ellipse(hc, hw / 2 + 8, hh / 2 + 8, fill=(40, 26, 16))
        v.ellipse(hc, hw / 2 + 4, hh / 2 + 4, fill=(230, 214, 176))
        v.ellipse(hc, hw / 2, hh / 2, fill=(16, 10, 8))
    elif kind == "dShape":
        # half-ellipse: flat side toward the bridge, curved side reaching toward the neck
        fx = hc[0] + hw / 2
        pts = [(fx - hw * math.sin(math.radians(a)), hc[1] - hh / 2 * math.cos(math.radians(a))) for a in range(0, 181, 6)]
        v.poly(pts, fill=(16, 10, 8), outline=(230, 214, 176), width=3)
    elif kind == "fHoles":
        for side in (1, -1):
            c = (hc[0], hc[1] * side)
            pts, eyes, er = f_hole(c[0], c[1], hw, hh, 7 * (-side))
            v.poly(pts, fill=(16, 10, 8))
            for e in eyes:
                v.circle(e, er, fill=(16, 10, 8))
    elif kind == "resonatorCover":
        r = hw / 2
        v.circle(hc, r, fill=(196, 200, 206), outline=(90, 94, 100), width=1.5)
        v.circle(hc, r * 0.82, outline=(140, 144, 150), width=1.0)
        for i in range(-6, 7):
            for j in range(-6, 7):
                p = (hc[0] + i * r * 0.13, hc[1] + j * r * 0.13)
                if dist(p, hc) < r * 0.74 and (i + j) % 2 == 0:
                    v.circle(p, r * 0.025, fill=(60, 62, 66))
        v.circle(hc, r * 0.22, fill=(178, 182, 188), outline=(90, 94, 100))
    uk, uc, uw, uhh = s.upper_holes
    if uk != "none":
        for side in (1, -1):
            c = (uc[0], uc[1] * side)
            if uk == "fHoles":
                pts, eyes, er = f_hole(c[0], c[1], uw, uhh, 7 * (-side))
                v.poly(pts, fill=(16, 10, 8))
                for e in eyes:
                    v.circle(e, er, fill=(16, 10, 8))
            else:
                v.circle(c, uw / 2, fill=(40, 40, 44), outline=(200, 204, 210), width=2)

    metal = (190, 194, 200)
    # bridge / tailpiece ghosts
    br = ctx.get("bridge")
    tail_end = s.x0 + s.length
    at = s.at
    nstr = ctx.get("strings", 6 if fam != "bass" else 4)
    sp = ctx.get("saddle_spacing", 10.5 if fam != "bass" else 19.0)
    half_s = sp * (nstr - 1) / 2
    if br == "trem":
        v.poly([at(34, -36), at(34, 36), at(-12, 36), at(-12, -36)], fill=metal, outline=(60, 60, 64))
    elif br == "hardtail":
        v.poly([at(18, -36), at(18, 36), at(-26, 36), at(-26, -36)], fill=metal, outline=(60, 60, 64))
    elif br == "floyd":
        v.poly([at(24, -38), at(24, 38), at(-50, 38), at(-50, -38)], fill=(40, 40, 44), outline=(10, 10, 10))
    elif br == "tele":
        v.poly([at(72, -43), at(72, 43), at(-30, 43), at(-30, -43)], fill=metal, outline=(60, 60, 64))
    elif br in ("tom",):
        v.poly([at(5, -41), at(5, 41), at(-5, 41), at(-5, -41)], fill=metal, outline=(60, 60, 64))
    elif br == "pin":
        v.poly([at(10, -76), at(10, 76), at(-26, 76), at(-26, -76)], fill=(30, 20, 14), outline=(0, 0, 0))
        v.poly([at(2, -36), at(2, 36), at(-2, 36), at(-2, -36)], fill=(240, 232, 214))
        for i in range(nstr):
            v.circle(at(-12, -half_s + i * sp), 2.5, fill=(240, 232, 214))
    elif br == "tie":
        v.poly([at(6, -92), at(6, 92), at(-24, 92), at(-24, -92)], fill=(50, 26, 14), outline=(0, 0, 0))
        v.poly([at(2, -38), at(2, 38), at(-2, 38), at(-2, -38)], fill=(240, 232, 214))
    elif br == "floating":
        v.poly([at(8, -62), at(8, 62), at(-8, 62), at(-8, -62)], fill=(30, 20, 14), outline=(0, 0, 0))
    elif br == "moustache":
        pts = [at(6, -80), at(0, -40), at(6, 0), at(0, 40), at(6, 80), at(-6, 80), at(-10, 40), at(-4, 0), at(-10, -40), at(-6, -80)]
        v.poly(pts, fill=(30, 20, 14))
    elif br == "bass":
        v.poly([at(16, -38), at(16, 38), at(-58, 38), at(-58, -38)], fill=metal, outline=(60, 60, 64))
    tp = ctx.get("tail")
    if tp == "stopbar":
        v.poly([at(-40, -44), at(-40, 44), at(-52, 44), at(-52, -44)], fill=metal, outline=(60, 60, 64))
    elif tp == "trapeze":
        t0 = sx - 20
        v.poly([(t0 + 0, -30), (tail_end - 4, -18), (tail_end - 4, 18), (t0, 30)] if False else
               [at(-60, -34), at(-60, 34), (tail_end - 6, 16), (tail_end - 6, -16)],
               fill=metal, outline=(60, 60, 64))

    # pickups
    for X, kind in ctx.get("pickups", []):
        dims = {"sc": (18, 70), "hb": (40, 70), "mini": (22, 62), "p90": (32, 82), "tele": (18, 70),
                "j": (22, 90), "splitp": (22, 40), "mm": (40, 100), "filter": (40, 70), "fb": (28, 70)}[kind]
        a, w = dims
        if kind == "hb":
            v.poly([at(X + a / 2 + 4, -44), at(X + a / 2 + 4, 44), at(X - a / 2 - 4, 44), at(X - a / 2 - 4, -44)], fill=(226, 208, 160))
            v.poly([at(X + a / 2, -w / 2), at(X + a / 2, w / 2), at(X - a / 2, w / 2), at(X - a / 2, -w / 2)], fill=(200, 202, 206), outline=(70, 70, 74))
        elif kind == "splitp":
            v.poly([at(X + 11 + 5, -44), at(X + 11 + 5, 0), at(X - 11 + 5, 0), at(X - 11 + 5, -44)], fill=(20, 20, 20))
            v.poly([at(X + 11 - 5, 0), at(X + 11 - 5, 44), at(X - 11 - 5, 44), at(X - 11 - 5, 0)], fill=(20, 20, 20))
        else:
            col = (236, 232, 220) if kind in ("sc",) else (24, 24, 24) if kind in ("j", "mm", "p90") else (200, 202, 206)
            v.poly([at(X + a / 2, -w / 2), at(X + a / 2, w / 2), at(X - a / 2, w / 2), at(X - a / 2, -w / 2)], fill=col, outline=(40, 40, 40))

    # controls, selector, jack, strap buttons
    knob_r = 15 if fam == "bass" else 12.5
    for c in s.controls:
        v.circle(c, knob_r, fill=(26, 24, 22), outline=(210, 170, 90), width=1.2)
        v.line([c, (c[0] - knob_r * 0.8, c[1])], fill=(230, 220, 190), width=1.2)
    if s.selector:
        v.circle(s.selector, 8, fill=(206, 200, 184), outline=(40, 40, 40))
        v.circle(s.selector, 3, fill=(20, 20, 20))
    if s.jack and fam in ("electric", "bass"):
        v.circle(s.jack, 9, fill=metal, outline=(40, 40, 44))
        v.circle(s.jack, 4, fill=(20, 20, 20))
    for sb in s.straps:
        v.circle(sb, 5.5, fill=metal, outline=(30, 30, 30))

    # neck + fretboard + headstock
    heel = ctx.get("heel", 56)
    nut_w = ctx.get("nut", 43)
    nx = s.neck_x
    fb_col = (40, 26, 20) if ctx.get("board", "rosewood") == "rosewood" else (224, 196, 140)

    def w_at(x):
        t = (x - nut_x) / (nx - nut_x)
        return nut_w + (heel - nut_w) * t

    head_len = ctx.get("head_len", 190 if fam != "bass" else 230)
    hx0 = nut_x - head_len
    hw = ctx.get("head_w", 88 if fam != "bass" else 100)
    head = [(nut_x, -nut_w / 2), (nut_x - 30, -nut_w / 2 - 6), (hx0 + 30, -hw / 2), (hx0, -hw / 2 + 12),
            (hx0, hw / 2 - 12), (hx0 + 30, hw / 2), (nut_x - 30, nut_w / 2 + 6), (nut_x, nut_w / 2)]
    v.poly(head, fill=(40, 26, 18), outline=(10, 8, 6), width=1.2)
    tuners = 6 if fam != "bass" else 4
    for i in range(tuners // 2):
        tx = nut_x - 55 - i * (head_len - 70) / max(1, tuners // 2 - 1) * 0.8
        for side in (-1, 1):
            v.circle((tx, side * (hw / 2 - 10)), 7, fill=metal, outline=(40, 40, 40))
    v.poly([(nut_x, -w_at(nut_x) / 2), (nx, -w_at(nx) / 2), (nx, w_at(nx) / 2), (nut_x, w_at(nut_x) / 2)],
           fill=fb_col, outline=(10, 8, 6), width=1.0)
    v.rect(nut_x - 4, -nut_w / 2, nut_x, nut_w / 2, fill=(240, 232, 214))
    for n in range(1, 30):
        fx = sx - scale * 2 ** (-n / 12)
        if fx >= nx:
            break
        v.line([(fx, -w_at(fx) / 2), (fx, w_at(fx) / 2)], fill=(200, 200, 204), width=1.0)
        if n in (3, 5, 7, 9, 15, 17, 19, 21):
            pfx = sx - scale * 2 ** (-(n - 0.5) / 12)
            v.circle((pfx, 0), 3.2, fill=(236, 230, 214))
        if n == 12:
            pfx = sx - scale * 2 ** (-(n - 0.5) / 12)
            v.circle((pfx, -10), 3.2, fill=(236, 230, 214))
            v.circle((pfx, 10), 3.2, fill=(236, 230, 214))

    # strings
    for i in range(nstr):
        y_n = -nut_w / 2 + 3.5 + i * (nut_w - 7) / (nstr - 1)
        y_s = -half_s + i * sp
        v.line([(nut_x, y_n), (sx, y_s), (sx + 12, y_s)], fill=(214, 216, 220, 200), width=0.7 if fam != "bass" else 1.1)

    # saddle line and markers
    v.line([(sx, -s.width / 2 - 8), (sx, s.width / 2 + 8)], fill=(255, 70, 50, 200), width=1.0)
    v.line([(nx, -s.width / 2 - 8), (nx, -s.width / 2 - 20)], fill=(90, 200, 255, 230), width=2.0)

    if label:
        jx = s.joint_x()
        jf = s.fret_of_x(jx) if jx is not None else float("nan")
        nf = s.fret_of_x(nx)
        v.text((hx0, -s.width / 2 - 50), s.id, 15, bold=True)
        v.text((hx0, -s.width / 2 - 30),
               "%d x %d mm  (phys %d)   saddleU %.3f   joint fret %.1f   neckU fret %.1f   err %.2f mm   %d pts"
               % (round(s.length), round(s.width), round(s.phys_width), s.uv((sx, 0))[0], jf, nf, s.error, len(s.outline)),
               11, fill=(185, 165, 138))


def guitar_extent(s):
    scale = s.ctx.get("scale", 648.0)
    head_len = s.ctx.get("head_len", 190 if s.family != "bass" else 230)
    left = s.saddle_x - scale - head_len
    right = s.x0 + s.length
    return left, right


def render_sheet(styles, path, px_per_mm=0.62, cols=2, ss=3):
    from PIL import Image
    ext = [guitar_extent(s) for s in styles]
    cell_w = max(r - l for l, r in ext) * px_per_mm + 60
    rows = int(math.ceil(len(styles) / cols))
    row_h = []
    for r in range(rows):
        row = styles[r * cols:(r + 1) * cols]
        row_h.append(max(s.width for s in row) * px_per_mm + 110)
    W, H = int(cell_w * cols + 40), int(sum(row_h) + 40)
    img = Image.new("RGB", (W * ss, H * ss), BG)
    y = 20
    for r in range(rows):
        for c in range(cols):
            i = r * cols + c
            if i >= len(styles):
                break
            s = styles[i]
            l, _ = ext[i]
            ox = 20 + c * cell_w + 30 - l * px_per_mm
            oy = y + 70 + s.width / 2 * px_per_mm
            draw_guitar(img, s, ox, oy, px_per_mm, ss)
        y += row_h[r]
    img = img.resize((W, H), Image.LANCZOS)
    img.save(path)


def render_single(s, path, px_per_mm=1.25, ss=3):
    from PIL import Image
    l, r = guitar_extent(s)
    body_l = s.saddle_x - s.ctx.get("scale", 648.0) * 0.55
    W = int((r - body_l) * px_per_mm + 60)
    H = int(s.width * px_per_mm + 130)
    img = Image.new("RGB", (W * ss, H * ss), BG)
    ox = 30 - body_l * px_per_mm
    oy = 90 + s.width / 2 * px_per_mm
    draw_guitar(img, s, ox, oy, px_per_mm, ss)
    img = img.resize((W, H), Image.LANCZOS)
    img.save(path)


# ----------------------------------------------------------------------------------------------
# Headstocks (guitar-illustration.md 6)
#
# Authored in millimetres in the header's own frame: a from the nut toward the tip, b lateral
# with the bass side negative. The outline runs from the nut's treble corner (0, +nut) round
# the tip to the nut's bass corner (0, -nut); both nut corners are sharp so the nut edge is
# straight. Drawn for a 43 mm nut (38 mm on basses), as the renderer expects.

HEAD_HEADER = os.path.join(ROOT, "Source", "UI", "Guitar", "HeadstockOutlines.h")
HEADS = []


def head(fn):
    HEADS.append(fn)
    return fn


class Head:
    """One headstock. posts / buttons: tuner post and key centres, index 0 = the lowest
    string. button: (length, width) of a key. tree: string tree centre or None."""

    def __init__(self, id, layout, outline, posts, buttons=None, button=(16, 11), truss=None, slots=None,
                 tree=None, bass=False, note="", strings=None):
        self.id, self.layout = id, layout
        self.knots, self.posts, self.buttons = outline, posts, buttons
        self.button, self.truss, self.slots, self.tree = button, truss, slots, tree
        self.bass, self.note = bass, note
        self.strings = strings or len(posts)

    def build(self):
        segs = segments(self.knots, True)
        self.outline, self.corners = resample(segs, True, max_len=30.0, max_turn=20.0)
        self.corners[0] = True                                         # the nut corners
        self.corners[-1] = True
        self.smooth = smooth(self.outline, True, False, corners=self.corners)
        self.truss_pts = None
        if self.truss:
            # the renderer smooths the cover without corners: sample densely instead
            self.truss_pts, _ = resample(segments(self.truss, True), True, max_len=6.0, max_turn=12.0)
        # slots are drawn as straight-sided polygons: sample their round ends densely
        self.slot_pts = [resample(segments(sl, True), True, max_len=8.0, max_turn=12.0)[0] for sl in (self.slots or [])]
        return self

    def nut_slots(self):
        """Where each string crosses the nut, lowest string first."""
        half = 19.0 if self.bass else 21.5
        edge = 4.5 if self.bass else 3.5
        if self.layout == "sixSix":                                    # six courses of two
            out = []
            for k in range(6):
                c = -half + edge + k * (2 * (half - edge) / 5)
                out += [c - 1.6, c + 1.6]
            return out
        n = self.strings
        return [-half + edge + i * (2 * (half - edge) / (n - 1)) for i in range(n)]

    def check(self):
        from shapely.geometry import LineString, Point, Polygon
        face = Polygon(self.smooth[:-1]).buffer(0)
        warn = []
        if not Polygon(self.smooth[:-1]).is_valid:
            warn.append("outline self-intersects")
        post_r = 5.0 if self.bass else 3.8
        margin = 0.0 if self.layout == "headless" else post_r + 1.5
        for i, p in enumerate(self.posts):
            if not face.buffer(-margin).contains(Point(p)):
                warn.append("post %d at (%.0f, %.0f) is not well inside the face" % (i, p[0], p[1]))
        side_keys = self.layout not in ("headless",)
        if self.buttons and side_keys and self.layout != "slotted":
            for i, bt in enumerate(self.buttons):
                if face.contains(Point(bt)):
                    warn.append("button %d sits on the face" % i)
        if self.layout == "headless":
            return warn
        lines = []
        for i, (y, p) in enumerate(zip(self.nut_slots(), self.posts)):
            ln = LineString([(0.0, y), p])
            lines.append(ln)
            outside = ln.difference(face.buffer(0.6))
            if outside.length > 1.0:
                warn.append("string %d leaves the face for %.1f mm" % (i, outside.length))
            # a string should not run through another string's post
            for j, q in enumerate(self.posts if self.layout != "slotted" else []):
                if j != i and ln.distance(Point(q)) < post_r - 0.5 and dist(q, (0.0, y)) < dist(p, (0.0, y)):
                    warn.append("string %d runs through post %d" % (i, j))
        for i in range(len(lines)):
            for j in range(i + 1, len(lines)):
                if lines[i].crosses(lines[j]):
                    warn.append("strings %d and %d cross" % (i, j))
        return warn


def key_out(post, normal_deg, reach):
    """A tuner key `reach` mm from its post, along a direction (degrees, a toward the tip)."""
    u = unit(normal_deg)
    return (post[0] + u[0] * reach, post[1] + u[1] * reach)


def straight_pull(first, last, n):
    """n posts evenly on the line first -> last (Fender-style straight string pull)."""
    return [(first[0] + (last[0] - first[0]) * i / (n - 1), first[1] + (last[1] - first[1]) * i / (n - 1)) for i in range(n)]


def edge_normal(p, q):
    """The outward (bass side) normal of the bass edge running p -> q, in degrees."""
    ang = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
    return ang - 90.0


def bell_cover(a0=6.0, a1=46.0, w0=5.0, w1=12.5):
    """A bell-shaped truss rod cover, narrow end at the nut."""
    m = a0 + (a1 - a0) * 0.55
    return [K(a0, -w0, 160, ao=90, h=0.3), K(a0, w0, 90, ao=20, h=0.3),
            K(m, w0 + (w1 - w0) * 0.5, 18), K(a1 - 5, w1, 5, h=0.4), K(a1, 0, -90, h=0.5),
            K(a1 - 5, -w1, 185, h=0.4), K(m, -w0 - (w1 - w0) * 0.5, 162)]


def rect_cover(a0, a1, w):
    return [K(a0, -w, 90, h=0.3), K(a0, w, 0, h=0.3), K(a1, w, -90, h=0.3), K(a1, -w, 180, h=0.3)]


# ---- in-line, Fender-style straight string pull -----------------------------------------

@head
def strat_inline6():
    # Straight bass edge slanting ~17 deg toward the treble side, so each string runs almost
    # straight to its post (the high-E post sits on the treble side of the centre line);
    # S-curved treble edge with a belly near the nut, a hook and a round bulb at the tip.
    # Proportions from a 1:1 luthier template.
    edge0, edge1 = (40.0, -32.5), (171.0, 6.5)
    outline = [
        K(0, 21.5, 90, ao=4, h=0.3),
        K(22, 26.5, 25),
        K(40, 38.6, 38),
        K(55, 46.3, 0),
        K(90, 39.6, -14),
        K(118, 32.0, -12),
        K(127.5, 30.4, 0, h=0.3),
        K(135.6, 40.7, 62),
        K(147, 49.8, 25),
        K(160, 52.4, 0),
        K(176, 48.1, -40),
        K(183.5, 34, -80),
        K(182.5, 20, -105),
        K(edge1[0], edge1[1], -150, ao=196.6, hin=0.35, hout=0),
        K(edge0[0], edge0[1], 196.6, hin=0),
        K(27, -35.6, 180),
        K(16, -30.2, 130),
        K(6, -23.2, 115),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = straight_pull((42, -18), (155, 15), 6)
    n = edge_normal(edge0, edge1)
    return Head("strat_inline6", "inline6", outline, posts, [key_out(p, n, 27) for p in posts], (16, 11),
                tree=(96, 12.5))


@head
def tele_inline6():
    # Smaller slanted paddle: the same straight-pull post line, a straight treble edge and a
    # blunt end cut back toward the bass side with a rounded treble corner.
    edge0, edge1 = (36.0, -31.5), (150.0, 3.0)
    outline = [
        K(0, 21.5, 90, ao=4, h=0.3),
        K(18, 25.5, 28),
        K(42, 30.5, 5),
        K(110, 31.2, 0),
        K(150, 32.5, 3),
        K(166, 27.5, -60, h=0.42),
        K(169, 15, -100),
        K(163, 7, -140, h=0.3),
        K(edge1[0], edge1[1], -168, ao=196.8, hin=0.3, hout=0),
        K(edge0[0], edge0[1], 196.8, hin=0),
        K(24, -33.8, 180),
        K(12, -28, 125),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = straight_pull((40, -18.5), (151, 13), 6)
    n = edge_normal(edge0, edge1)
    return Head("tele_inline6", "inline6", outline, posts, [key_out(p, n, 26) for p in posts], (16, 11),
                tree=(98, 12))


@head
def offset_inline6():
    # The larger Fender offset headstock: the Strat's geometry with a fuller treble belly
    # and a bigger bulb.
    edge0, edge1 = (40.0, -33.5), (178.0, 7.0)
    outline = [
        K(0, 21.5, 90, ao=4, h=0.3),
        K(22, 27.5, 26),
        K(42, 41.5, 36),
        K(60, 50.5, 0),
        K(95, 43.5, -14),
        K(124, 35.0, -12),
        K(133, 33.4, 0, h=0.3),
        K(142, 44.5, 62),
        K(155, 54.5, 25),
        K(170, 57.5, 0),
        K(186, 52.0, -42),
        K(193, 36, -82),
        K(191, 20, -108),
        K(edge1[0], edge1[1], -150, ao=196.4, hin=0.35, hout=0),
        K(edge0[0], edge0[1], 196.4, hin=0),
        K(27, -36.4, 180),
        K(16, -30.8, 130),
        K(6, -23.4, 115),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = straight_pull((42, -18.5), (158, 15.5), 6)
    n = edge_normal(edge0, edge1)
    return Head("offset_inline6", "inline6", outline, posts, [key_out(p, n, 27) for p in posts], (16, 11),
                tree=(98, 12.5))


def pointy(L, n, first, slant=6.0, spacing=22.4):
    """Pointed in-line headstock (superstrat family): posts on a line slanting `slant`
    degrees toward the treble side, a straight bass edge parallel to it, and a long treble
    sweep curving up to a point at the end of the bass edge. Returns (outline, posts, keys)."""
    tn = math.tan(math.radians(slant))
    last = (first[0] + spacing * (n - 1), first[1] + tn * spacing * (n - 1))
    posts = straight_pull(first, last, n)

    def edge_b(a):
        return first[1] - 11.5 / math.cos(math.radians(slant)) + tn * (a - first[0])

    tip = (L, edge_b(L))
    back = 180.0 + slant
    outline = [
        K(0, 21.5, 90, ao=4, h=0.3),
        K(20, 24.5, 20),
        K(50, 27.5, 4),
        K(L * 0.55, 29.0, 0),
        K(L * 0.80, 24.0, -18),
        K(L * 0.93, 10.0, -45),
        K(tip[0], tip[1], -62, ao=back, hin=0.35, hout=0),
        K(34, edge_b(34), back, hin=0),
        K(18, edge_b(34) - 0.5, 165),
        K(7, -24.5, 118),
        K(0, -21.5, 90, h=0.3),
    ]
    keys = [key_out(p, slant - 90.0, 25) for p in posts]
    return outline, posts, keys


@head
def superstrat_inline6():
    # Pointed, drooping in-line headstock: long treble sweep curving up to a point at the
    # end of a straight bass edge; posts on a line slanting 6 deg toward the treble side.
    outline, posts, keys = pointy(204.0, 6, (40, -18.5))
    return Head("superstrat_inline6", "inline6", outline, posts, keys, (14, 10))


@head
def inline7():
    # Seven-string pointed in-line headstock.
    outline, posts, keys = pointy(226.0, 7, (38, -19.5), slant=6.5, spacing=21.0)
    return Head("inline7", "inline6", outline, posts, keys, (14, 10))


@head
def inline8():
    # Eight-string pointed in-line headstock.
    outline, posts, keys = pointy(248.0, 8, (36, -20.0), slant=7.0, spacing=20.5)
    return Head("inline8", "inline6", outline, posts, keys, (14, 10))


@head
def explorer_hockey():
    # "Hockey stick": long and narrow, tuners along a straight bass edge that slants toward
    # the treble side, the end hooked toward the treble side like a stick's blade.
    edge0, edge1 = (30.0, -30.0), (178.0, 10.0)
    outline = [
        K(0, 21.5, 90, ao=6, h=0.3),
        K(16, 24.5, 18),
        K(40, 27.5, 5),
        K(150, 33, 5),
        K(196, 40, 12),
        K(222, 44, 0, ao=-105, hin=0.3, hout=0.3),
        K(218, 30, -130),
        K(200, 20, -160),
        K(edge1[0], edge1[1], -163, ao=195.1, hin=0.3, hout=0),
        K(edge0[0], edge0[1], 195.1, hin=0),
        K(16, -30, 160),
        K(6, -24, 118),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = straight_pull((40, -17.5), (160, 15), 6)
    n = edge_normal(edge0, edge1)
    return Head("explorer_hockey", "inline6", outline, posts, [key_out(p, n, 25) for p in posts], (15, 11),
                truss=bell_cover(8, 36, 4.5, 9.5))


@head
def firebird_inline6():
    # Firebird-style in-line headstock: banjo tuners along the bass edge (keys seen edge-on),
    # a long straight treble edge and a round crown on the bass side.
    edge0, edge1 = (34.0, -29.0), (190.0, -17.0)
    outline = [
        K(0, 21.5, 90, ao=4, h=0.3),
        K(20, 26, 22),
        K(45, 31, 6),
        K(150, 33, -2),
        K(196, 27, -30),
        K(214, 6, -75, h=0.42),
        K(208, -18, -125),
        K(edge1[0], edge1[1], -175, ao=184.4, hin=0.3, hout=0),
        K(edge0[0], edge0[1], 184.4, hin=0),
        K(16, -29.5, 160),
        K(6, -24, 118),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = straight_pull((44, -18), (176, -7), 6)
    return Head("firebird_inline6", "inline6", outline, posts, [key_out(p, -94, 20) for p in posts], (10, 8))


# ---- 3+3 ---------------------------------------------------------------------------------

def gibson_posts(first_a, spacing, b0, flare):
    bass = [(first_a + i * spacing, -(b0 + i * flare)) for i in range(3)]
    treble = [(first_a + i * spacing, b0 + i * flare) for i in range(3)]
    return bass + list(reversed(treble))


@head
def lp_33():
    # Open-book 3+3: flares from the nut to ~68 mm, near-parallel sides widening slightly to
    # ~78 mm, square-ish top corners and a top edge rising to two peaks either side of a
    # small central notch; ~170 mm long. From a 1:1 luthier template.
    outline = [
        K(0, 21.5, 90, ao=8, h=0.3),
        K(12, 24.0, 22),
        K(24, 29.6, 40),
        K(33, 34.2, 12),
        K(60, 33.6, 0),
        K(100, 34.4, 2),
        K(157, 39.2, 3, ao=-68, hin=0.3, hout=0.3),
        K(164.5, 27.5, -80),
        K(169.5, 9.5, -90, h=0.35),
        K(167, 0, -130, ao=-50, hin=0.3, hout=0.3),
        K(169.5, -9.5, -90, h=0.35),
        K(164.5, -27.5, -100),
        K(157, -39.2, -112, ao=177, hin=0.3, hout=0.3),
        K(100, -34.4, 178),
        K(60, -33.6, 180),
        K(33, -34.2, 168),
        K(24, -29.6, 140),
        K(12, -24.0, 158),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = gibson_posts(52, 37, 25.0, 1.0)
    return Head("lp_33", "threeThree", outline, posts, [(p[0], p[1] + math.copysign(28, p[1])) for p in posts], (16, 12),
                truss=bell_cover(6, 46, 5, 12.5))


@head
def sg_33():
    # The same open book, a touch narrower and shorter.
    outline = [
        K(0, 21.5, 90, ao=8, h=0.3),
        K(12, 23.8, 22),
        K(24, 28.8, 40),
        K(32, 33.2, 12),
        K(60, 32.8, 0),
        K(100, 33.6, 2),
        K(152, 37.8, 3, ao=-68, hin=0.3, hout=0.3),
        K(159, 26.5, -80),
        K(164, 9, -90, h=0.35),
        K(161.5, 0, -130, ao=-50, hin=0.3, hout=0.3),
        K(164, -9, -90, h=0.35),
        K(159, -26.5, -100),
        K(152, -37.8, -112, ao=177, hin=0.3, hout=0.3),
        K(100, -33.6, 178),
        K(60, -32.8, 180),
        K(32, -33.2, 168),
        K(24, -28.8, 140),
        K(12, -23.8, 158),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = gibson_posts(51, 36, 24.5, 1.0)
    return Head("sg_33", "threeThree", outline, posts, [(p[0], p[1] + math.copysign(28, p[1])) for p in posts], (16, 12),
                truss=bell_cover(6, 44, 5, 12))


@head
def v_33():
    # Flying-V "arrowhead": narrow 3+3 whose sides run out to a point on the centre line.
    outline = [
        K(0, 21.5, 90, ao=8, h=0.3),
        K(14, 24.5, 25),
        K(30, 31.5, 18),
        K(110, 36.5, 2),
        K(142, 35, -15),
        K(198, 0, -32, ao=-148, hin=0.25, hout=0.25),
        K(142, -35, 195),
        K(110, -36.5, 178),
        K(30, -31.5, 162),
        K(14, -24.5, 155),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = gibson_posts(48, 36, 24.0, 0.5)
    return Head("v_33", "threeThree", outline, posts, [(p[0], p[1] + math.copysign(28, p[1])) for p in posts], (16, 12),
                truss=bell_cover(6, 42, 4.5, 11))


@head
def acoustic_33():
    # Square flat-top headstock: quick shoulders at the nut, straight sides flaring slightly
    # toward the tip, small-radius corners and a straight top edge.
    outline = [
        K(0, 21.5, 90, ao=10, h=0.3),
        K(10, 26.5, 45),
        K(20, 33.5, 20),
        K(40, 35.2, 2),
        K(160, 38.8, 2),
        K(172, 35.5, -70, h=0.45),
        K(174.5, 20, -90),
        K(174.5, -20, -90),
        K(172, -35.5, -110, h=0.45),
        K(160, -38.8, 178),
        K(40, -35.2, 178),
        K(20, -33.5, 160),
        K(10, -26.5, 135),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = gibson_posts(50, 35, 26.0, 0.8)
    return Head("acoustic_33", "threeThree", outline, posts, [(p[0], p[1] + math.copysign(28, p[1])) for p in posts], (15, 11),
                truss=rect_cover(4, 22, 7))


@head
def archtop_33():
    # Larger, ornate flared headstock: concave sides widening to ~94 mm, rounded top corners
    # and a top edge with a raised centre between two shallow scallops.
    outline = [
        K(0, 21.5, 90, ao=8, h=0.3),
        K(14, 25, 25),
        K(40, 33, 12),
        K(100, 39.5, 8),
        K(160, 46.5, 10),
        K(184, 46, -30, h=0.42),
        K(192, 34, -95),
        K(188, 20, -80),
        K(196, 9, -60),
        K(200, 0, -90, h=0.4),
        K(196, -9, -120),
        K(188, -20, -100),
        K(192, -34, -85),
        K(184, -46, -150, h=0.42),
        K(160, -46.5, 170),
        K(100, -39.5, 172),
        K(40, -33, 168),
        K(14, -25, 155),
        K(0, -21.5, 90, h=0.3),
    ]
    posts = gibson_posts(56, 38, 27.0, 2.5)
    return Head("archtop_33", "threeThree", outline, posts, [(p[0], p[1] + math.copysign(28, p[1])) for p in posts], (17, 12),
                truss=bell_cover(6, 46, 5, 12.5))


@head
def twelve_66():
    # Twelve-string 6+6: the square acoustic headstock stretched to take six tuners a side;
    # posts in course pairs, lowest course nearest the nut.
    outline = [
        K(0, 21.5, 90, ao=10, h=0.3),
        K(10, 27, 45),
        K(22, 35.5, 18),
        K(45, 37.5, 2),
        K(214, 41, 1),
        K(226, 37.5, -70, h=0.45),
        K(229, 20, -90),
        K(229, -20, -90),
        K(226, -37.5, -110, h=0.45),
        K(214, -41, 179),
        K(45, -37.5, 178),
        K(22, -35.5, 162),
        K(10, -27, 135),
        K(0, -21.5, 90, h=0.3),
    ]
    bass = [(40 + i * 22.5, -(27.0 + i * 0.3)) for i in range(6)]
    treble = [(40 + i * 22.5, 27.0 + i * 0.3) for i in range(6)]
    # course k (0 = lowest) uses posts 2k and 2k+1: courses 0-2 on the bass side, 3-5 treble
    posts = bass + list(reversed(treble))
    return Head("twelve_66", "sixSix", outline, posts, [(p[0], p[1] + math.copysign(25, p[1])) for p in posts], (13, 10),
                truss=rect_cover(4, 22, 7), strings=12)


# ---- slotted -----------------------------------------------------------------------------

def slot(a0, a1, b0, b1):
    """A rectangular slot with round ends."""
    r = (b1 - b0) / 2
    c = (b0 + b1) / 2
    return [K(a0, c, 90, h=0.55), K(a0 + r, b1, 0, h=0.3), K(a1 - r, b1, 0, h=0.3), K(a1, c, -90, h=0.55),
            K(a1 - r, b0, 180, h=0.3), K(a0 + r, b0, 180, h=0.3)]


@head
def classical_slotted():
    # Spanish slotted headstock: parallel sides ~58 mm apart, two slots with three rollers
    # each (35 mm spacing), keys out the sides, and a scalloped crown at the top.
    outline = [
        K(0, 21.5, 90, ao=12, h=0.3),
        K(9, 26, 40),
        K(22, 29, 5),
        K(170, 29.5, 0),
        K(182, 26, -60, h=0.4),
        K(181, 16, -120),
        K(186, 9, -40),
        K(194, 0, -90, h=0.45),
        K(186, -9, -140),
        K(181, -16, -60),
        K(182, -26, -120, h=0.4),
        K(170, -29.5, 180),
        K(22, -29, 175),
        K(9, -26, 140),
        K(0, -21.5, 90, h=0.3),
    ]
    rollers = [60, 95.5, 131]
    posts = [(a, -15.5) for a in rollers] + [(a, 15.5) for a in reversed(rollers)]
    return Head("classical_slotted", "slotted", outline, posts, [(p[0], p[1] + math.copysign(26, p[1])) for p in posts],
                (14, 10), slots=[slot(42, 150, -20.5, -10.5), slot(42, 150, 10.5, 20.5)])


@head
def gypsy_slotted():
    # Selmer-style slotted headstock: a little narrower, gently flared, with a rounded arch
    # top rising to a soft point.
    outline = [
        K(0, 21.5, 90, ao=12, h=0.3),
        K(9, 25, 35),
        K(24, 27.5, 5),
        K(150, 29.5, 2),
        K(172, 29, -20),
        K(186, 18, -60),
        K(193, 0, -90, h=0.42),
        K(186, -18, -120),
        K(172, -29, -160),
        K(150, -29.5, 178),
        K(24, -27.5, 175),
        K(9, -25, 145),
        K(0, -21.5, 90, h=0.3),
    ]
    rollers = [58, 93.5, 129]
    posts = [(a, -15.5) for a in rollers] + [(a, 15.5) for a in reversed(rollers)]
    return Head("gypsy_slotted", "slotted", outline, posts, [(p[0], p[1] + math.copysign(26, p[1])) for p in posts],
                (14, 10), slots=[slot(40, 148, -20.5, -10.5), slot(40, 148, 10.5, 20.5)])


@head
def resonator_slotted():
    # Resonator slotted headstock: square shoulders, straight sides and a flat top with
    # rounded corners.
    outline = [
        K(0, 21.5, 90, ao=12, h=0.3),
        K(8, 27, 55),
        K(18, 31, 8),
        K(168, 31.5, 0),
        K(180, 28, -60, h=0.45),
        K(183, 12, -90),
        K(183, -12, -90),
        K(180, -28, -120, h=0.45),
        K(168, -31.5, 180),
        K(18, -31, 172),
        K(8, -27, 125),
        K(0, -21.5, 90, h=0.3),
    ]
    rollers = [62, 97.5, 133]
    posts = [(a, -16) for a in rollers] + [(a, 16) for a in reversed(rollers)]
    return Head("resonator_slotted", "slotted", outline, posts, [(p[0], p[1] + math.copysign(27, p[1])) for p in posts],
                (14, 10), slots=[slot(44, 152, -21, -11), slot(44, 152, 11, 21)])


# ---- basses ------------------------------------------------------------------------------

@head
def bass_inline4():
    # Fender-style bass headstock: the Strat geometry scaled up - straight bass edge slanting
    # toward the treble side so the strings pull straight, big keys, S-curved treble edge
    # and a round bulb; ~230 mm long for a 38 mm nut.
    edge0, edge1 = (46.0, -34.0), (212.0, 8.0)
    outline = [
        K(0, 19.0, 90, ao=4, h=0.3),
        K(26, 23.5, 22),
        K(50, 36.0, 36),
        K(70, 44.0, 0),
        K(112, 38.0, -12),
        K(152, 31.5, -8),
        K(162, 31.2, 5, h=0.3),
        K(172, 41.0, 60),
        K(186, 50.5, 25),
        K(200, 53.0, 0),
        K(218, 48.0, -45),
        K(226, 32, -82),
        K(223, 18, -110),
        K(edge1[0], edge1[1], -150, ao=194.2, hin=0.35, hout=0),
        K(edge0[0], edge0[1], 194.2, hin=0),
        K(32, -36.5, 180),
        K(18, -30.5, 128),
        K(6, -21.5, 112),
        K(0, -19.0, 90, h=0.3),
    ]
    posts = straight_pull((52, -15.5), (190, 14.5), 4)
    n = edge_normal(edge0, edge1)
    return Head("bass_inline4", "fourInline", outline, posts, [key_out(p, n, 36) for p in posts], (22, 18),
                tree=(118, 10.0), bass=True)


@head
def bass_inline5():
    # Five-string version: longer, the same straight-pull geometry.
    edge0, edge1 = (46.0, -35.0), (246.0, 10.0)
    outline = [
        K(0, 19.0, 90, ao=4, h=0.3),
        K(26, 23.5, 22),
        K(52, 36.5, 36),
        K(74, 44.5, 0),
        K(125, 38.5, -11),
        K(180, 31.5, -8),
        K(191, 31.2, 5, h=0.3),
        K(202, 41.5, 60),
        K(217, 51.0, 25),
        K(232, 53.5, 0),
        K(251, 48.5, -45),
        K(260, 32, -82),
        K(257, 18, -110),
        K(edge1[0], edge1[1], -150, ao=192.7, hin=0.35, hout=0),
        K(edge0[0], edge0[1], 192.7, hin=0),
        K(32, -37.5, 180),
        K(18, -31, 128),
        K(6, -21.5, 112),
        K(0, -19.0, 90, h=0.3),
    ]
    posts = straight_pull((50, -16.5), (222, 16.0), 5)
    n = edge_normal(edge0, edge1)
    return Head("bass_inline5", "fourInline", outline, posts, [key_out(p, n, 36) for p in posts], (22, 18),
                tree=(154, 12.0), bass=True)


@head
def bass_22():
    # 2+2 bass headstock for hollow and violin basses: flared sides, rounded shoulders at
    # the top and a gently domed top edge.
    outline = [
        K(0, 19.0, 90, ao=10, h=0.3),
        K(14, 23.5, 38),
        K(34, 32.5, 16),
        K(120, 38.5, 3),
        K(150, 37.5, -25, h=0.42),
        K(160, 24, -80),
        K(164, 0, -90, h=0.5),
        K(160, -24, -100),
        K(150, -37.5, -155, h=0.42),
        K(120, -38.5, 177),
        K(34, -32.5, 164),
        K(14, -23.5, 142),
        K(0, -19.0, 90, h=0.3),
    ]
    posts = [(60, -24), (110, -26.5), (110, 26.5), (60, 24)]
    return Head("bass_22", "twoTwo", outline, posts, [(p[0], p[1] + math.copysign(32, p[1])) for p in posts], (22, 18),
                truss=bell_cover(6, 42, 4.5, 11), bass=True)


@head
def bass_31():
    # 3+1 bass headstock: three keys along the bass side, the G key alone on the treble side
    # near the nut; a rounded, asymmetric outline that is widest at the treble key.
    outline = [
        K(0, 19.0, 90, ao=8, h=0.3),
        K(16, 24, 35),
        K(40, 33.5, 20),
        K(70, 38.5, 0),
        K(102, 34, -20),
        K(150, 24, -15),
        K(176, 14, -45),
        K(184, -4, -95, h=0.45),
        K(176, -24, -140),
        K(150, -35, -175),
        K(90, -36.5, 180),
        K(40, -33.5, 170),
        K(16, -25, 145),
        K(0, -19.0, 90, h=0.3),
    ]
    posts = [(56, -23.5), (100, -25.0), (144, -24.0), (72, 24.5)]
    buttons = [(56, -57), (100, -59), (144, -58), (72, 57)]
    return Head("bass_31", "threeOne", outline, posts, buttons, (22, 18), bass=True)


# ---- headless -----------------------------------------------------------------------------

@head
def headless():
    # Headless guitar: a short end cap carrying the string anchors (tuners are at the bridge).
    outline = [K(0, 21.5, 90, ao=0, h=0.3), K(12, 23, 0), K(16, 18, -80, h=0.4), K(16, -18, -90, h=0.4),
               K(12, -23, 180), K(0, -21.5, 90, h=0.3)]
    posts = [(9, -18 + i * 7.2) for i in range(6)]
    return Head("headless", "headless", outline, posts, None, (0, 0))


@head
def headless_bass():
    # Headless bass: the end cap sized for four strings on a 38 mm nut.
    outline = [K(0, 19.0, 90, ao=0, h=0.3), K(13, 21, 0), K(18, 16, -80, h=0.4), K(18, -16, -90, h=0.4),
               K(13, -21, 180), K(0, -19.0, 90, h=0.3)]
    posts = [(10, -14.5 + i * 9.67) for i in range(4)]
    return Head("headless_bass", "headless", outline, posts, None, (0, 0), bass=True)


DEFAULT_HEADSTOCK = """/** The headstock a body style carries by default (guitar-illustration.md 6). Never null. */
inline const char* defaultHeadstockFor (const char* bodyStyleId, int numStrings) noexcept
{
    auto is = [bodyStyleId] (const char* s) { return bodyStyleId != nullptr && std::strcmp (bodyStyleId, s) == 0; };
    auto startsWith = [bodyStyleId] (const char* s) { return bodyStyleId != nullptr && std::strncmp (bodyStyleId, s, std::strlen (s)) == 0; };

    if (numStrings >= 12)                                             return "twelve_66";
    if (is ("headless_bass"))                                         return "headless_bass";

    if (startsWith ("bass") || is ("acoustic_bass"))
    {
        if (is ("bass_hollow") || is ("bass_violin") || is ("acoustic_bass"))  return "bass_22";
        if (is ("bass_musicman"))                                    return "bass_31";
        return numStrings >= 5 ? "bass_inline5" : "bass_inline4";
    }

    if (numStrings == 7)                                             return "inline7";
    if (numStrings >= 8)                                             return "inline8";

    if (is ("double_cutaway_offset"))                                return "strat_inline6";
    if (is ("offset"))                                               return "offset_inline6";
    if (is ("reverse_firebird"))                                     return "firebird_inline6";
    if (is ("single_cutaway_slab"))                                  return "tele_inline6";
    if (is ("superstrat") || is ("multiscale"))                      return "superstrat_inline6";
    if (is ("angular"))                                              return "explorer_hockey";
    if (is ("flying_v"))                                             return "v_33";
    if (is ("double_cutaway_thin"))                                  return "sg_33";
    if (is ("archtop"))                                              return "archtop_33";
    if (is ("single_cutaway_arched") || is ("double_cutaway_semi"))  return "lp_33";
    if (startsWith ("gypsy_jazz"))                                   return "gypsy_slotted";
    if (is ("classical") || is ("flamenco") || is ("cutaway_classical"))  return "classical_slotted";
    if (startsWith ("resonator"))                                    return "resonator_slotted";

    return "acoustic_33";
}
"""


def num(v):
    s = ("%.2f" % v).rstrip("0").rstrip(".")
    if s == "-0":
        s = "0"
    return s + ("f" if "." in s else "")


def hpt(p, corner=False):
    return "{ %s, %s%s }" % (num(p[0]), num(p[1]), ", true" if corner else "")


def hp_list(pts, corners=None, per_line=6):
    corners = corners or [False] * len(pts)
    items = [hpt(p, c) for p, c in zip(pts, corners)]
    return ",\n".join("        " + ", ".join(items[i:i + per_line]) for i in range(0, len(items), per_line))


HEAD_TOP = """#pragma once
// Generated by Tools/body_outlines.py - do not edit by hand.

/*  Headstock outlines (guitar-illustration.md 6), as data.

    Coordinates per headstock, in millimetres:
      a  from the nut toward the tip (a >= 0);
      b  lateral, NEGATIVE = bass side (the top of the screen), positive = treble.
    Drawn for a 43 mm nut (38 mm on basses): the renderer rescales b near the
    nut to the guitar's real nut width.

    `outline` is closed, from the nut's treble corner round the tip to the nut's
    bass corner, smoothed with the same centripetal Catmull-Rom as the bodies
    (a `corner` point keeps straight tangents; both nut corners are corners, so
    the nut edge is straight). `posts` and `buttons` are in string order with
    index 0 the LOWEST string; the renderer maps them onto the engine's order
    (string 0 = high E). twelve_66 pairs its posts by course: 2k and 2k + 1 are
    course k, lowest course first. Slotted headstocks: posts are the roller
    centres, buttons the keys out the sides. Headless: posts are the string
    anchors and there are no buttons.

    In-line Fender-style headstocks use a straight string pull: the post line
    slants toward the treble side so every string runs almost straight from its
    nut slot to its post (the high-E post sits on the treble side of centre).
*/

#include <cstring>

namespace luthier::outlines
{

struct HeadPt { float a, b; bool corner = false; };

enum class HeadLayout { inline6, inlineReverse, threeThree, twoTwo, threeOne, fourInline, slotted, sixSix, headless };

struct HeadstockStyle
{
    const char* id;
    HeadLayout layout;
    const HeadPt* outline; int numOutline;
    const HeadPt* posts; int numPosts;
    const HeadPt* buttons; int numButtons;
    float buttonLengthMm, buttonWidthMm;
    const HeadPt* trussCover; int numTrussCover;
    const HeadPt* slots[2]; int numSlot[2];
    HeadPt stringTree; bool hasStringTree;
};

//==============================================================================
namespace headstock_data
{
"""


def emit_heads(heads):
    out = [HEAD_TOP]
    for h in heads:
        c = h.id
        out.append("    // %s%s\n" % (c, (" - " + h.note) if h.note else ""))
        for line in h.doc:
            out.append("    //   %s\n" % line)
        out.append("    inline constexpr HeadPt %s_outline[] = {\n%s };\n" % (c, hp_list(h.outline, h.corners)))
        out.append("    inline constexpr HeadPt %s_posts[] = {\n%s };\n" % (c, hp_list(h.posts)))
        if h.buttons:
            out.append("    inline constexpr HeadPt %s_buttons[] = {\n%s };\n" % (c, hp_list(h.buttons)))
        if h.truss_pts:
            out.append("    inline constexpr HeadPt %s_truss[] = {\n%s };\n" % (c, hp_list(h.truss_pts)))
        for k, pts in enumerate(h.slot_pts):
            out.append("    inline constexpr HeadPt %s_slot%d[] = {\n%s };\n" % (c, k, hp_list(pts)))
        out.append("\n")
    out.append("    template <typename T, int N> constexpr int count (const T (&)[N]) { return N; }\n}\n\n")

    out.append("//==============================================================================\n")
    out.append("inline constexpr HeadstockStyle kHeadstockStyles[] = {\n")
    for h in heads:
        c = h.id
        tree = hpt(h.tree) if h.tree else "{ 0, 0 }"
        fields = [
            '"%s", HeadLayout::%s,' % (c, h.layout),
            "headstock_data::%s_outline, headstock_data::count (headstock_data::%s_outline)," % (c, c),
            "headstock_data::%s_posts, %d," % (c, len(h.posts)),
            ("headstock_data::%s_buttons, %d, %s, %s," % (c, len(h.buttons), num(h.button[0]), num(h.button[1])))
            if h.buttons else "nullptr, 0, 0, 0,",
            ("headstock_data::%s_truss, headstock_data::count (headstock_data::%s_truss)," % (c, c))
            if h.truss_pts else "nullptr, 0,",
            ("{ headstock_data::%s_slot0, headstock_data::%s_slot1 }, { headstock_data::count (headstock_data::%s_slot0), "
             "headstock_data::count (headstock_data::%s_slot1) }," % (c, c, c, c)) if h.slots else "{ nullptr, nullptr }, { 0, 0 },",
            "%s, %s }," % (tree, "true" if h.tree else "false"),
        ]
        out.append("    { " + "\n      ".join(fields) + "\n")
    out.append("};\n\n")
    out.append("inline constexpr int kNumHeadstockStyles = (int) (sizeof (kHeadstockStyles) / sizeof (kHeadstockStyles[0]));\n\n")
    out.append("""/** The headstock with this id, or nullptr if there is none. */
inline const HeadstockStyle* findHeadstockStyle (const char* id) noexcept
{
    if (id == nullptr)
        return nullptr;

    for (int i = 0; i < kNumHeadstockStyles; ++i)
        if (std::strcmp (kHeadstockStyles[i].id, id) == 0)
            return &kHeadstockStyles[i];

    return nullptr;
}

""")
    out.append(DEFAULT_HEADSTOCK)
    out.append("\n} // namespace luthier::outlines\n")
    with open(HEAD_HEADER, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("".join(out))


def draw_head(img, h, ox, oy, s, ss):
    """A headstock seen as the renderer shows it: nut on the right, tip to the left, bass up."""
    from PIL import ImageDraw, ImageFilter, Image
    d = ImageDraw.Draw(img, "RGBA")

    def P(p):
        return ((ox - p[0] * s) * ss, (oy + p[1] * s) * ss)

    def poly(pts, fill=None, outline=None, width=1.0):
        q = [P(p) for p in pts]
        if fill is not None:
            d.polygon(q, fill=fill)
        if outline is not None:
            d.line(q + [q[0]], fill=outline, width=max(1, int(width * ss)), joint="curve")

    def circle(c, r, fill=None, outline=None, width=1.0):
        x, y = P(c)
        rr = r * s * ss
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=fill, outline=outline, width=max(1, int(width * ss)))

    half = 19.0 if h.bass else 21.5
    # fretboard stub and nut
    poly([(0, -half), (-90, -half - 2), (-90, half + 2), (0, half)], fill=(40, 26, 20), outline=(10, 8, 6))
    for fa in (-36, -70):
        d.line([P((fa, -half - 1)), P((fa, half + 1))], fill=(200, 200, 204), width=max(1, int(1.0 * ss)))
    # shadow + face
    face = (22, 17, 14) if not h.id.startswith(("strat", "tele", "offset", "bass_inline")) else (227, 189, 127)
    poly(h.smooth, fill=face, outline=(8, 6, 4), width=1.3)
    for pts in h.slot_pts:
        poly(pts, fill=(10, 7, 5))
    if h.truss_pts:
        poly(smooth(h.truss_pts, True, False), fill=(18, 18, 18), outline=(233, 227, 214), width=1.0)
    poly([(0, -half), (-4, -half), (-4, half), (0, half)], fill=(240, 232, 214))
    metal = (196, 200, 206)
    # keys and shafts
    if h.buttons:
        for p, b in zip(h.posts, h.buttons):
            if h.layout != "slotted":
                d.line([P(p), P(b)], fill=metal, width=max(1, int(3.2 * s * ss)))
            ang = math.atan2(b[1] - p[1], b[0] - p[0])
            L, W = h.button
            pts = []
            for k in range(24):
                t = 2 * math.pi * k / 24
                x, y = L / 2 * math.cos(t), W / 2 * math.sin(t)
                pts.append((b[0] + x * math.cos(ang) - y * math.sin(ang), b[1] + x * math.sin(ang) + y * math.cos(ang)))
            poly(pts, fill=(240, 230, 210) if h.layout == "slotted" else metal, outline=(60, 60, 64))
    # strings
    for i, (y, p) in enumerate(zip(h.nut_slots(), h.posts)):
        if h.layout == "headless":
            continue
        d.line([P((-90, y)), P((0, y)), P(p)], fill=(226, 228, 232, 230), width=max(1, int((1.4 if h.bass else 0.8) * s * ss)))
    # posts / rollers / tree
    for p in h.posts:
        if h.layout == "slotted":
            poly([(p[0] - 3.2, p[1] - 9), (p[0] + 3.2, p[1] - 9), (p[0] + 3.2, p[1] + 9), (p[0] - 3.2, p[1] + 9)],
                 fill=(240, 230, 210), outline=(120, 110, 90))
        else:
            r = 5.0 if h.bass else 3.8
            circle(p, r + 2, fill=(150, 154, 160), outline=(50, 50, 54))
            circle(p, r, fill=(220, 224, 228))
    if h.tree:
        c = h.tree
        poly([(c[0] - 3, c[1] - 7), (c[0] + 3, c[1] - 7), (c[0] + 3, c[1] + 7), (c[0] - 3, c[1] + 7)], fill=metal,
             outline=(40, 40, 44))


def render_heads(heads, path, s=1.9, cols=4, ss=3):
    from PIL import Image, ImageDraw
    cell_w, cell_h = int(360 * s), int(170 * s)
    rows = int(math.ceil(len(heads) / cols))
    W, H = cell_w * cols, cell_h * rows
    img = Image.new("RGB", (W * ss, H * ss), BG)
    d = ImageDraw.Draw(img)
    for i, h in enumerate(heads):
        cx, cy = (i % cols) * cell_w, (i // cols) * cell_h
        ox = cx + cell_w - 100 * s
        oy = cy + cell_h * 0.55
        draw_head(img, h, ox, oy, s, ss)
        warn = h.check()
        d.text(((cx + 10) * ss, (cy + 8) * ss), h.id + ("   (%d warnings)" % len(warn) if warn else ""),
               font=font(int(15 * ss), True), fill=(239, 227, 204) if not warn else (255, 120, 90))
    img = img.resize((W, H), Image.LANCZOS)
    img.save(path)


# ----------------------------------------------------------------------------------------------

def main(argv):
    only = None
    png = "--no-png" not in argv
    for a in argv:
        if a.startswith("--only"):
            only = set(a.split("=", 1)[1].split(",")) if "=" in a else None
    if "--only" in argv:
        only = set(argv[argv.index("--only") + 1].split(","))

    styles = []
    for fn in STYLES:
        s = fn().build()
        s.doc = describe(fn)
        styles.append(s)
    ids = [s.id for s in styles]
    assert len(ids) == len(set(ids)), "duplicate style id"
    emit_header(styles)

    print("%-26s %-10s %6s %6s %6s  %6s %6s  %5s %5s  %5s %4s  %s" %
          ("style", "family", "len", "width", "phys", "saddU", "neckU", "jFret", "nFret", "err", "pts", "spec"))
    for s in styles:
        for w in s.check():
            print("  ! %s: %s" % (s.id, w))
    for s in styles:
        jx = s.joint_x()
        print("%-26s %-10s %6.1f %6.1f %6.1f  %6.3f %6.3f  %5.1f %5.1f  %5.2f %4d  %s" %
              (s.id, s.family, s.length, s.width, s.phys_width, s.uv((s.saddle_x, 0))[0], s.uv((s.neck_x, 0))[0],
               s.fret_of_x(jx) if jx is not None else -1, s.fret_of_x(s.neck_x), s.error, len(s.outline),
               ("%d x %d" % s.spec) if s.spec else ""))

    heads = []
    for fn in HEADS:
        h = fn().build()
        h.doc = describe(fn)
        heads.append(h)
    assert len({h.id for h in heads}) == len(heads), "duplicate headstock id"
    emit_heads(heads)
    for h in heads:
        for w in h.check():
            print("  ! %s: %s" % (h.id, w))
    print("%d headstocks: %s" % (len(heads), ", ".join("%s (%d pts)" % (h.id, len(h.outline)) for h in heads)))

    if png:
        os.makedirs(PREVIEWS, exist_ok=True)
        render_heads(heads, os.path.join(PREVIEWS, "headstocks.png"))
        for h in heads:
            if only and h.id not in only:
                continue
            render_heads([h], os.path.join(PREVIEWS, "styles", "head_" + h.id + ".png"), s=3.2, cols=1)
        os.makedirs(os.path.join(PREVIEWS, "styles"), exist_ok=True)
        fams = {}
        for s in styles:
            fams.setdefault(s.family, []).append(s)
        for fam, ss_ in fams.items():
            if only and not any(s.id in only for s in ss_):
                continue
            render_sheet(ss_, os.path.join(PREVIEWS, fam + ".png"))
        for s in styles:
            if only and s.id not in only:
                continue
            render_single(s, os.path.join(PREVIEWS, "styles", s.id + ".png"))


if __name__ == "__main__":
    main(sys.argv[1:])
