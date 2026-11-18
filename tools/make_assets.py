"""Maakt de app-plaatjes uit het logo van de gebruiker (assets/logo_source.jpg).

Uitvoer (in Resources/):
  bg_alley.jpg      - achtergrond (origineel, 1408x768 = precies schermhoogte iPad 2)
  logo_cutout.png   - uitgeknipt logo (witte letters + zwarte rand + rode gloed), RGBA
  logo_cutout.txt   - positie van de uitsnede in het origineel: x y w h
  Icon-*.png        - app-iconen
Gebruik: py -3.12 tools/make_assets.py
"""
import os
import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "logo_source.jpg")
OUT = os.path.join(ROOT, "Resources")

im = Image.open(SRC).convert("RGB")
W, H = im.size
a = np.asarray(im).astype(np.float32)
lum = a.mean(axis=2)
sat = a.max(axis=2) - a.min(axis=2)

# Witte letters: licht en weinig kleur. Daarna alleen grote samenhangende vlakken houden,
# zodat losse lichte baksteentjes en de lamp wegvallen.
from scipy import ndimage
white = (lum > 120) & (sat < 45)
white[:100, :] = False
white[660:, :] = False
lab, n = ndimage.label(ndimage.binary_closing(white, iterations=2))
sizes = ndimage.sum(np.ones_like(lum), lab, index=range(1, n + 1))
keep = np.zeros(n + 1, bool)
for i, sz in enumerate(sizes, start=1):
    if sz > 2500:
        keep[i] = True
white = keep[lab] & white
ys, xs = np.nonzero(white)
x0, x1 = xs.min(), xs.max()
y0, y1 = ys.min(), ys.max()
print("letters:", x0, y0, x1, y1)

mask = Image.fromarray((white * 255).astype(np.uint8))
mask = mask.filter(ImageFilter.MaxFilter(5))           # gaatjes in letters dicht
outline = mask.filter(ImageFilter.MaxFilter(13))        # zwarte rand eromheen
outline = outline.filter(ImageFilter.GaussianBlur(1.2))
glow = mask.filter(ImageFilter.MaxFilter(15)).filter(ImageFilter.GaussianBlur(14))

pad = 40
bx0, by0 = max(0, x0 - pad), max(0, y0 - pad)
bx1, by1 = min(W, x1 + pad), min(H, y1 + pad)

src = np.asarray(im).astype(np.float32)
o = np.asarray(outline).astype(np.float32) / 255.0
g = np.asarray(glow).astype(np.float32) / 255.0 * 0.55
m = np.asarray(mask.filter(ImageFilter.GaussianBlur(0.8))).astype(np.float32) / 255.0

# kleur: binnen de letters het origineel (krasjes blijven), daarbuiten zwart; gloed rood
rgb = np.zeros_like(src)
rgb[..., 0] = 150.0
alpha = np.maximum(o, g)
letter_rgb = src * m[..., None] + np.array([12, 8, 8]) * (1 - m[..., None])
w_out = o / np.maximum(alpha, 1e-4)
rgb = letter_rgb * w_out[..., None] + rgb * (1 - w_out[..., None])
rgba = np.dstack([np.clip(rgb, 0, 255), np.clip(alpha * 255, 0, 255)]).astype(np.uint8)
cut = Image.fromarray(rgba, "RGBA").crop((bx0, by0, bx1, by1))
os.makedirs(OUT, exist_ok=True)
cut.save(os.path.join(OUT, "logo_cutout.png"), optimize=True)
with open(os.path.join(OUT, "logo_cutout.txt"), "w") as f:
    f.write("%d %d %d %d\n" % (bx0, by0, bx1 - bx0, by1 - by0))
print("cutout:", bx0, by0, bx1 - bx0, by1 - by0)

im.save(os.path.join(OUT, "bg_alley.jpg"), quality=88, optimize=True)

# Iconen: vierkant rond het logo
cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
s = min(H, int((x1 - x0) * 1.08))
sq = im.crop((cx - s // 2, max(0, cy - s // 2), cx - s // 2 + s, max(0, cy - s // 2) + s))
for size in (29, 40, 50, 57, 58, 72, 76, 80, 100, 114, 120, 144, 152):
    sq.resize((size, size), Image.LANCZOS).save(os.path.join(OUT, "Icon-%d.png" % size))
print("klaar")
