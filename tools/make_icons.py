"""Tekent de kaart-icoontjes (eigen pictogrammen) naar Resources/icons.png.
Volgorde (64 x 64 elk, naast elkaar): 0 wapenwinkel, 1 bank, 2 autodealer, 3 customs, 4 missie, 5 missie gehaald.
Gebruik: py -3.12 tools/make_icons.py
"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 4          # supersampling
N = 64 * S
W = (255, 255, 255, 255)
K = (20, 20, 22, 255)


def badge(color):
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([2 * S, 2 * S, N - 2 * S, N - 2 * S], fill=K)
    d.ellipse([6 * S, 6 * S, N - 6 * S, N - 6 * S], fill=color)
    return im, d


def P(*pts):
    return [(x * S, y * S) for x, y in pts]


icons = []

# 0: pistool
im, d = badge((205, 30, 30, 255))
d.polygon(P((16, 24), (48, 24), (48, 31), (30, 31), (28, 34), (25, 46), (17, 46), (20, 31), (16, 31)), fill=W)
d.rectangle(P((26, 31), (32, 37)), outline=W, width=2 * S)
icons.append(im)

# 1: bank (zuilen + fronton + $)
im, d = badge((40, 150, 70, 255))
d.polygon(P((32, 12), (52, 23), (12, 23)), fill=W)
for x in (18, 27, 37, 46):
    d.rectangle(P((x - 2.5, 25), (x + 2.5, 44)), fill=W)
d.rectangle(P((12, 45), (52, 50)), fill=W)
icons.append(im)

# 2: auto
im, d = badge((240, 130, 20, 255))
d.polygon(P((12, 38), (14, 30), (22, 29), (27, 22), (40, 22), (46, 29), (52, 31), (52, 38)), fill=W)
d.polygon(P((25, 29), (29, 24), (38, 24), (42, 29)), fill=(240, 130, 20, 255))
d.ellipse(P((17, 34), (27, 44)), fill=K)
d.ellipse(P((38, 34), (48, 44)), fill=K)
d.ellipse(P((20, 37), (24, 41)), fill=W)
d.ellipse(P((41, 37), (45, 41)), fill=W)
icons.append(im)

# 3: moersleutel
im, d = badge((40, 110, 230, 255))
d.line(P((20, 44), (40, 24)), fill=W, width=7 * S)
d.ellipse(P((34, 14), (52, 32)), fill=W)
d.polygon(P((43, 13), (53, 13), (53, 23), (46, 23)), fill=(40, 110, 230, 255))
d.ellipse(P((15, 39), (25, 49)), fill=W)
icons.append(im)

# 4: missie (!)
im, d = badge((245, 200, 30, 255))
d.rounded_rectangle(P((28, 13), (36, 38)), radius=3 * S, fill=K)
d.ellipse(P((27, 42), (37, 52)), fill=K)
icons.append(im)

# 5: missie gehaald (vinkje)
im, d = badge((90, 200, 90, 255))
d.line(P((18, 33), (28, 43), (47, 21)), fill=W, width=7 * S, joint="curve")
icons.append(im)

atlas = Image.new("RGBA", (64 * len(icons), 64), (0, 0, 0, 0))
for i, ic in enumerate(icons):
    atlas.alpha_composite(ic.resize((64, 64), Image.LANCZOS), (i * 64, 0))
out = os.path.join(ROOT, "Resources", "icons.png")
atlas.save(out)
print("klaar:", out, atlas.size)
