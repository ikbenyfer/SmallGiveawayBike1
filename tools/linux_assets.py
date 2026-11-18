"""Zet plaatjes en een lettertype-atlas klaar voor de Linux-testopstelling (geen UIKit daar).
Uitvoer in <outdir>: *.raw (int32 w, int32 h, premultiplied RGBA) + font.raw + font.txt
"""
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[1]
os.makedirs(out, exist_ok=True)


def write_raw(img, path):
    img = img.convert("RGBA")
    w, h = img.size
    px = bytearray(img.tobytes())
    for i in range(0, len(px), 4):
        a = px[i + 3]
        if a != 255:
            px[i] = px[i] * a // 255
            px[i + 1] = px[i + 1] * a // 255
            px[i + 2] = px[i + 2] * a // 255
    with open(path, "wb") as f:
        f.write(struct.pack("<ii", w, h))
        f.write(px)


for name in ("bg_alley.jpg", "logo_cutout.png", "nl_hb_map.png"):
    write_raw(Image.open(os.path.join(root, "Resources", name)), os.path.join(out, name + ".raw"))

size = 48
font_path = None
for cand in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
             "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"):
    if os.path.exists(cand):
        font_path = cand
font = ImageFont.truetype(font_path, size) if font_path else ImageFont.load_default()
W, H = 1024, 512
pad = 6
img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
asc, desc = font.getmetrics()
lineh = asc + desc
cellh = lineh + pad * 2
x, y = 1, 1
lines = []
for c in range(32, 128):
    ch = "?" if c == 127 else chr(c)
    adv = int(round(font.getlength(ch)))
    cw = adv + pad * 2
    if x + cw > W - 1:
        x = 1
        y += cellh + 1
    d.text((x + pad, y + pad), ch, font=font, fill=(255, 255, 255, 255), stroke_width=2, stroke_fill=(0, 0, 0, 255))
    lines.append("%f %f %f %f %f %f %f %f %f" % (x / W, y / H, (x + cw) / W, (y + cellh) / H, cw, cellh, -pad, -pad, adv))
    x += cw + 1
write_raw(img, os.path.join(out, "font.raw"))
with open(os.path.join(out, "font.txt"), "w") as f:
    f.write("%f %f\n" % (size, lineh))
    f.write("\n".join(lines) + "\n")
print("klaar:", os.listdir(out))
