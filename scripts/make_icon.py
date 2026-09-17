"""Generates the unique Luthier application/plugin icon.

Motif: a guitar soundhole + rosette with six strings crossing it, rendered in the
plugin's warm amber-on-walnut identity. Reads clearly all the way down to 16 px
because the silhouette is a single high-contrast circle plus vertical lines.
"""
import math, os
from PIL import Image, ImageDraw, ImageFilter

S = 1024                      # supersampled working size
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources")

BG_OUTER = (20, 17, 14)
BG_INNER = (46, 36, 28)
AMBER    = (224, 135, 60)
AMBER_HI = (255, 196, 116)
PATINA   = (111, 165, 160)
STRING   = (214, 205, 190)

def radial(size, inner, outer, cx, cy, r):
    img = Image.new("RGB", (size, size), outer)
    px = img.load()
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - cx, y - cy) / r
            t = min(1.0, d)
            t = t * t
            px[x, y] = tuple(int(inner[i] + (outer[i] - inner[i]) * t) for i in range(3))
    return img

def main():
    os.makedirs(OUT, exist_ok=True)
    img = radial(S, BG_INNER, BG_OUTER, S * 0.42, S * 0.36, S * 0.85).convert("RGBA")
    d = ImageDraw.Draw(img, "RGBA")

    cx = cy = S // 2
    hole_r = int(S * 0.255)

    # --- rosette: concentric rings, alternating amber / patina, widening outward
    for i, (rr, col, w) in enumerate([
        (hole_r + int(S * 0.085), AMBER + (70,),  int(S * 0.012)),
        (hole_r + int(S * 0.062), PATINA + (90,), int(S * 0.008)),
        (hole_r + int(S * 0.044), AMBER + (150,), int(S * 0.016)),
        (hole_r + int(S * 0.022), AMBER + (200,), int(S * 0.009)),
    ]):
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=col, width=w)

    # --- rosette mosaic: small radial ticks on the widest amber ring
    rr = hole_r + int(S * 0.044)
    for k in range(48):
        a = 2 * math.pi * k / 48
        x0, y0 = cx + math.cos(a) * (rr - S * 0.011), cy + math.sin(a) * (rr - S * 0.011)
        x1, y1 = cx + math.cos(a) * (rr + S * 0.011), cy + math.sin(a) * (rr + S * 0.011)
        d.line([x0, y0, x1, y1], fill=BG_OUTER + (190,), width=int(S * 0.006))

    # --- the soundhole itself: a deep well with a lit upper rim
    d.ellipse([cx - hole_r, cy - hole_r, cx + hole_r, cy + hole_r], fill=(8, 6, 5, 255))
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.arc([cx - hole_r, cy - hole_r, cx + hole_r, cy + hole_r], 190, 350,
           fill=AMBER_HI + (210,), width=int(S * 0.018))
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.012))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img, "RGBA")
    d.ellipse([cx - hole_r, cy - hole_r, cx + hole_r, cy + hole_r],
              outline=(0, 0, 0, 255), width=int(S * 0.006))

    # --- six strings, gauged: thick low E on the left thinning to high E
    span = S * 0.60
    for i in range(6):
        x = cx - span / 2 + span * i / 5.0
        w = int(S * (0.0125 - 0.0014 * i))
        d.line([x, S * 0.045, x, S * 0.955], fill=(0, 0, 0, 150), width=w + int(S * 0.005))
        d.line([x, S * 0.045, x, S * 0.955], fill=STRING + (255,), width=w)
        d.line([x - w * 0.30, S * 0.045, x - w * 0.30, S * 0.955],
               fill=(255, 255, 255, 120), width=max(1, int(w * 0.34)))

    # --- brand notch (the signature element), top-left, 45deg
    n = int(S * 0.11)
    d.line([int(S * 0.055), int(S * 0.055) + n, int(S * 0.055) + n, int(S * 0.055)],
           fill=AMBER + (255,), width=int(S * 0.020))

    img = img.convert("RGB")
    big = img.resize((512, 512), Image.LANCZOS)
    big.save(os.path.join(OUT, "icon.png"))
    img.resize((128, 128), Image.LANCZOS).save(os.path.join(OUT, "icon_small.png"))

    ico = os.path.join(OUT, "luthier.ico")
    big.save(ico, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("wrote icon.png (512), icon_small.png (128), luthier.ico")

if __name__ == "__main__":
    main()
