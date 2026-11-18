"""Bouwt de ingebouwde kaart van Heemskerk + Beverwijk (Resources/nl_hb.pak + Resources/nl_hb_map.png).

Bronnen (open data, CC BY 4.0):
  - 3D BAG (TU Delft / 3DGI) WFS: gebouwen met hoogte (LoD 1.2)
  - PDOK TOP10NL (Kadaster): wegen met namen, water, terrein
  - PDOK Bestuurlijke gebieden: gemeentegrenzen
  - PDOK Locatieserver: postcodes en startpunten

Gebruik: py -3.12 tools/build_region_pack.py      (downloads worden bewaard in tools/cache/)

Pakketformaat (little endian), tegels van 250 x 250 m in RD:
  "SGBP" u32 versie=1, f64 ox, f64 oy (RD van tegel 0,0), i32 ntx, i32 nty, f32 tegelgrootte
  ntx*nty x (u32 offset, u32 lengte_gecomprimeerd, u32 lengte_ruw)   (offset 0 = lege tegel)
  u32 npc,   npc x (char[6] postcode, i32 x, i32 y)
  u32 nstart, nstart x (char[48] naam, i32 x, i32 y)
  daarna de zlib-blokken per tegel. Inhoud tegel (coordinaten i16 in dm t.o.v. tegelhoek):
  u16 nGeb  x (u16 hoogte_dm, u16 n, n*(i16,i16))
  u16 nWeg  x (u8 breedte_dm/2, u8 soort, u16 naam, u16 n, n*(i16,i16))
  u16 nWat  x (u16 n, n*(i16,i16))
  u16 nTer  x (u8 code, u16 n, n*(i16,i16))
  u16 nNaam x (u8 len, bytes)
"""
import json
import math
import os
import struct
import sys
import time
import urllib.parse
import urllib.request
import zlib

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", "cache")
OUT_PAK = os.path.join(ROOT, "Resources", "nl_hb.pak")
OUT_PNG = os.path.join(ROOT, "Resources", "nl_hb_map.png")
OUT_META = os.path.join(ROOT, "Resources", "nl_hb_map.txt")
TILE = 250.0
CRS = "http://www.opengis.net/def/crs/EPSG/0/28992"
UA = {"User-Agent": "SmallGiveawayBike1-regionpack/1.0"}
os.makedirs(CACHE, exist_ok=True)


def get_json(url, cache_name):
    p = os.path.join(CACHE, cache_name)
    if os.path.exists(p):
        with open(p, "r", encoding="utf-8") as f:
            return json.load(f)
    for attempt in range(5):
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=120) as r:
                data = r.read()
            d = json.loads(data.decode("utf-8"))
            with open(p, "wb") as f:
                f.write(data)
            time.sleep(0.15)
            return d
        except Exception as e:  # noqa: BLE001
            print("  opnieuw (%s): %s" % (cache_name, e))
            time.sleep(2 + attempt * 3)
    raise SystemExit("download mislukt: " + url)


# ------------------------------------------------------------------ gemeentegrenzen
def walk(c):
    if isinstance(c[0], (int, float)):
        yield c
    else:
        for x in c:
            yield from walk(x)


munis = []
for g in ("Heemskerk", "Beverwijk"):
    d = get_json("https://api.pdok.nl/kadaster/bestuurlijkegebieden/ogc/v1/collections/gemeentegebied/items?naam=%s&crs=%s&f=json" % (g, CRS), "gem_%s.json" % g)
    geom = d["features"][0]["geometry"]
    rings = []
    if geom["type"] == "Polygon":
        rings.append(geom["coordinates"][0])
    else:
        for poly in geom["coordinates"]:
            rings.append(poly[0])
    munis.append(rings)


def in_ring(x, y, ring):
    inside = False
    n = len(ring)
    j = n - 1
    for i in range(n):
        xi, yi = ring[i][0], ring[i][1]
        xj, yj = ring[j][0], ring[j][1]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi:
            inside = not inside
        j = i
    return inside


def in_region(x, y):
    return any(in_ring(x, y, r) for rings in munis for r in rings)


allpts = [p for rings in munis for r in rings for p in r]
minx, miny = min(p[0] for p in allpts), min(p[1] for p in allpts)
maxx, maxy = max(p[0] for p in allpts), max(p[1] for p in allpts)
tx0, ty0 = int(math.floor(minx / TILE)), int(math.floor(miny / TILE))
tx1, ty1 = int(math.floor(maxx / TILE)), int(math.floor(maxy / TILE))
ntx, nty = tx1 - tx0 + 1, ty1 - ty0 + 1
print("regio RD %.0f,%.0f - %.0f,%.0f : %d x %d tegels" % (minx, miny, maxx, maxy, ntx, nty))

tiles_in = set()
for ty in range(ty0, ty1 + 1):
    for tx in range(tx0, tx1 + 1):
        hit = False
        for a in range(5):
            for b in range(5):
                if in_region((tx + (a + 0.5) / 5) * TILE, (ty + (b + 0.5) / 5) * TILE):
                    hit = True
                    break
            if hit:
                break
        if hit:
            tiles_in.add((tx, ty))
# ook een rand van 1 tegel rondom (strand, buurgemeenten aan de grens)
grow = set()
for (tx, ty) in tiles_in:
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            grow.add((tx + dx, ty + dy))
tiles_in = {t for t in grow if tx0 <= t[0] <= tx1 and ty0 <= t[1] <= ty1}
print("tegels in de regio:", len(tiles_in))

# ------------------------------------------------------------------ downloaden per blok van 1 km
BLOCK = 4  # tegels per blok
blocks = sorted({((tx - tx0) // BLOCK, (ty - ty0) // BLOCK) for (tx, ty) in tiles_in})
print("blokken van 1 km:", len(blocks))

buildings = {}   # id -> (h, ring)
roads = {}       # id -> (props, lines)
waters = {}      # id -> rings
terrains = {}    # id -> (code, rings)


def feats_paged(base, cache_prefix):
    out = []
    url = base
    page = 0
    while url:
        d = get_json(url, "%s_p%d.json" % (cache_prefix, page))
        out.extend(d.get("features", []))
        nxt = None
        for l in d.get("links", []):
            if l.get("rel") == "next":
                nxt = l.get("href")
        url = nxt
        page += 1
    return out


def polys_of(geom):
    if not geom:
        return []
    if geom["type"] == "Polygon":
        return [geom["coordinates"][0]]
    if geom["type"] == "MultiPolygon":
        return [p[0] for p in geom["coordinates"]]
    return []


def lines_of(geom):
    if not geom:
        return []
    if geom["type"] == "LineString":
        return [geom["coordinates"]]
    if geom["type"] == "MultiLineString":
        return list(geom["coordinates"])
    return []


LANDUSE = [("grasland", 1), ("bos", 2), ("akker", 3), ("bebouwd", 4), ("zand", 5), ("kwekerij", 6),
           ("boomgaard", 6), ("dodenakker", 7), ("spoor", 8), ("heide", 9)]


def landuse_code(t):
    if not t:
        return 0
    for k, c in LANDUSE:
        if k in t:
            return c
    return 0


for bi, (bx, by) in enumerate(blocks):
    x0, y0 = (tx0 + bx * BLOCK) * TILE, (ty0 + by * BLOCK) * TILE
    x1, y1 = x0 + BLOCK * TILE, y0 + BLOCK * TILE
    tag = "%d_%d" % (int(x0), int(y0))
    sys.stdout.write("\rblok %d/%d %s   " % (bi + 1, len(blocks), tag))
    sys.stdout.flush()
    # 3D BAG met paginering
    start = 0
    while True:
        url = ("https://data.3dbag.nl/api/BAG3D/wfs?service=WFS&version=2.0.0&request=GetFeature&typeNames=BAG3D:lod12"
               "&outputFormat=application/json&srsName=EPSG:28992&bbox=%d,%d,%d,%d,EPSG:28992"
               "&propertyName=geom,b3_h_70p,b3_h_maaiveld,identificatie&count=5000&startIndex=%d" % (x0, y0, x1, y1, start))
        d = get_json(url, "bag_%s_%d.json" % (tag, start))
        fs = d.get("features", [])
        for f in fs:
            p = f.get("properties", {})
            fid = p.get("identificatie") or f.get("id")
            if fid in buildings:
                continue
            h = (p.get("b3_h_70p") or 9.0) - (p.get("b3_h_maaiveld") or 0.0)
            ps = polys_of(f.get("geometry"))
            if ps:
                buildings[fid] = (max(2.5, min(160.0, h)), ps[0])
        if len(fs) < 5000:
            break
        start += 5000
    q = "bbox=%d,%d,%d,%d&bbox-crs=%s&crs=%s&limit=1000&f=json" % (x0, y0, x1, y1, urllib.parse.quote(CRS, safe=""), urllib.parse.quote(CRS, safe=""))
    for f in feats_paged("https://api.pdok.nl/brt/top10nl/ogc/v1/collections/wegdeel_hartlijn/items?" + q, "road_" + tag):
        fid = f.get("id") or f["properties"].get("lokaalid")
        if fid not in roads:
            roads[fid] = (f.get("properties", {}), lines_of(f.get("geometry")))
    for f in feats_paged("https://api.pdok.nl/brt/top10nl/ogc/v1/collections/waterdeel_vlak/items?" + q, "water_" + tag):
        fid = f.get("id")
        if fid not in waters:
            waters[fid] = polys_of(f.get("geometry"))
    for f in feats_paged("https://api.pdok.nl/brt/top10nl/ogc/v1/collections/terrein_vlak/items?" + q, "terrain_" + tag):
        fid = f.get("id")
        if fid not in terrains:
            terrains[fid] = (landuse_code(f.get("properties", {}).get("typelandgebruik")), polys_of(f.get("geometry")))
print("\ngebouwen %d, wegen %d, water %d, terrein %d" % (len(buildings), len(roads), len(waters), len(terrains)))

# ------------------------------------------------------------------ verdelen over tegels
def clip(ring, lo_x, lo_y, hi_x, hi_y):
    pts = [(p[0], p[1]) for p in ring]
    for e in range(4):
        out = []
        n = len(pts)
        if n == 0:
            break
        for i in range(n):
            a, b = pts[i], pts[(i + 1) % n]
            def val(p):
                return (p[0] - lo_x, hi_x - p[0], p[1] - lo_y, hi_y - p[1])[e]
            va, vb = val(a), val(b)
            if va >= 0:
                out.append(a)
            if (va >= 0) != (vb >= 0):
                t = va / (va - vb)
                out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        pts = out
    return pts


tiles = {t: {"b": [], "r": [], "w": [], "t": [], "names": []} for t in tiles_in}


def tile_of(x, y):
    return (int(math.floor(x / TILE)), int(math.floor(y / TILE)))


for fid, (h, ring) in buildings.items():
    cx = sum(p[0] for p in ring) / len(ring)
    cy = sum(p[1] for p in ring) / len(ring)
    t = tile_of(cx, cy)
    if t in tiles:
        tiles[t]["b"].append((h, ring))


def road_kind(p):
    g = (p.get("hoofdverkeersgebruik") or "")
    tw = (p.get("typeweg") or "")
    if "fiets" in g:
        return 1
    if "voetgang" in g:
        return 2
    if "hoofdweg" in tw or "autosnelweg" in tw or "regionale" in tw:
        return 3
    return 0


def road_width(p):
    br = p.get("verhardingsbreedteklasse") or ""
    if "< 2" in br:
        return 1.8
    if "2 - 4" in br:
        return 3.4
    if "4 - 7" in br:
        return 5.8
    if "> 7" in br:
        return 8.5
    return 4.5


for fid, (p, lines) in roads.items():
    for line in lines:
        cur_t, cur = None, []
        for i in range(len(line) - 1):
            a, b = line[i], line[i + 1]
            t = tile_of((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            if t != cur_t:
                if cur_t in tiles and len(cur) >= 2:
                    tiles[cur_t]["r"].append((p, cur))
                cur_t, cur = t, [a]
            cur.append(b)
        if cur_t in tiles and len(cur) >= 2:
            tiles[cur_t]["r"].append((p, cur))

for coll, key, has_code in ((waters, "w", False), (terrains, "t", True)):
    for fid, val in coll.items():
        code, rings = (val if has_code else (0, val))
        for ring in rings:
            xs = [q[0] for q in ring]
            ys = [q[1] for q in ring]
            for ty in range(int(min(ys) // TILE), int(max(ys) // TILE) + 1):
                for tx in range(int(min(xs) // TILE), int(max(xs) // TILE) + 1):
                    if (tx, ty) not in tiles:
                        continue
                    c = clip(ring, tx * TILE, ty * TILE, (tx + 1) * TILE, (ty + 1) * TILE)
                    if len(c) >= 3:
                        tiles[(tx, ty)][key].append((code, c))


# ------------------------------------------------------------------ binair schrijven
def q16(v):
    return max(-32767, min(32767, int(round(v * 10))))


def pts_bytes(pts, ox, oy, maxn=4000):
    pts = pts[:maxn]
    return struct.pack("<H", len(pts)) + b"".join(struct.pack("<hh", q16(p[0] - ox), q16(p[1] - oy)) for p in pts)


blobs = {}
raw_total = 0
for (tx, ty), d in tiles.items():
    ox, oy = tx * TILE, ty * TILE
    names = []
    name_idx = {}
    out = [struct.pack("<H", min(len(d["b"]), 65535))]
    for h, ring in d["b"][:65535]:
        r = ring[:-1] if len(ring) > 1 and ring[0][:2] == ring[-1][:2] else ring
        out.append(struct.pack("<H", int(round(h * 10))) + pts_bytes(r, ox, oy))
    out.append(struct.pack("<H", len(d["r"])))
    for p, line in d["r"]:
        nm = p.get("naam")
        if nm:
            if nm not in name_idx:
                name_idx[nm] = len(names)
                names.append(nm)
            ni = name_idx[nm]
        else:
            ni = 0xFFFF
        out.append(struct.pack("<BBH", int(round(road_width(p) * 5)), road_kind(p), ni) + pts_bytes(line, ox, oy))
    out.append(struct.pack("<H", len(d["w"])))
    for code, ring in d["w"]:
        out.append(pts_bytes(ring, ox, oy))
    out.append(struct.pack("<H", len(d["t"])))
    for code, ring in d["t"]:
        out.append(struct.pack("<B", code) + pts_bytes(ring, ox, oy))
    out.append(struct.pack("<H", len(names)))
    for nm in names:
        b = nm.encode("utf-8")[:255]
        out.append(struct.pack("<B", len(b)) + b)
    raw = b"".join(out)
    raw_total += len(raw)
    blobs[(tx, ty)] = (zlib.compress(raw, 9), len(raw))

# postcodes in de regio (voor offline zoeken)
pcs = {}
for g in ("Heemskerk", "Beverwijk"):
    start = 0
    while True:
        url = ("https://api.pdok.nl/bzk/locatieserver/search/v3_1/free?q=*&fq=type:postcode&fq=gemeentenaam:%s&rows=100&start=%d&fl=postcode,centroide_rd" % (g, start))
        d = get_json(url, "pc_%s_%d.json" % (g, start))
        docs = d["response"]["docs"]
        for doc in docs:
            pt = doc.get("centroide_rd", "")
            if "(" in pt and doc.get("postcode"):
                x, y = pt[pt.index("(") + 1:pt.index(")")].split()
                pcs[doc["postcode"]] = (float(x), float(y))
        start += 100
        if start >= d["response"]["numFound"] or not docs:
            break
print("postcodes:", len(pcs))

starts = []
for label, q in (("Beverwijk - centrum (Breestraat)", "Breestraat Beverwijk"),
                 ("Station Beverwijk", "Stationsplein Beverwijk"),
                 ("Heemskerk - centrum", "Maerten van Heemskerckplein Heemskerk"),
                 ("Station Heemskerk", "Stationsplein Heemskerk"),
                 ("Wijk aan Zee", "Voorstraat Wijk aan Zee"),
                 ("Beverwijkse Bazaar", "Montageweg Beverwijk")):
    d = get_json("https://api.pdok.nl/bzk/locatieserver/search/v3_1/free?q=%s&rows=1&fl=centroide_rd,weergavenaam" % urllib.parse.quote(q), "start_%s.json" % q.replace(" ", "_"))
    docs = d["response"]["docs"]
    if docs and "centroide_rd" in docs[0]:
        pt = docs[0]["centroide_rd"]
        x, y = pt[pt.index("(") + 1:pt.index(")")].split()
        starts.append((label, float(x), float(y)))
print("startpunten:", [s[0] for s in starts])

hdr = b"SGBP" + struct.pack("<I", 1) + struct.pack("<dd", tx0 * TILE, ty0 * TILE) + struct.pack("<iif", ntx, nty, TILE)
index_size = ntx * nty * 12
tail = struct.pack("<I", len(pcs)) + b"".join(pc.encode()[:6].ljust(6, b"\0") + struct.pack("<ii", int(x), int(y)) for pc, (x, y) in sorted(pcs.items()))
tail += struct.pack("<I", len(starts)) + b"".join(n.encode("utf-8")[:47].ljust(48, b"\0") + struct.pack("<ii", int(x), int(y)) for n, x, y in starts)
pos = len(hdr) + index_size + len(tail)
index = []
body = []
for ty in range(ty0, ty1 + 1):
    for tx in range(tx0, tx1 + 1):
        b = blobs.get((tx, ty))
        if b:
            index.append(struct.pack("<III", pos, len(b[0]), b[1]))
            body.append(b[0])
            pos += len(b[0])
        else:
            index.append(struct.pack("<III", 0, 0, 0))
with open(OUT_PAK, "wb") as f:
    f.write(hdr + b"".join(index) + tail + b"".join(body))
print("pakket: %s, %.1f MB (ruw %.1f MB)" % (OUT_PAK, os.path.getsize(OUT_PAK) / 1048576, raw_total / 1048576))

# ------------------------------------------------------------------ overzichtskaart (6 m per pixel)
MPP = 6.0
W = int(ntx * TILE / MPP)
H = int(nty * TILE / MPP)
img = Image.new("RGB", (W, H), (44, 46, 54))
dr = ImageDraw.Draw(img)
X0, Y1 = tx0 * TILE, (ty1 + 1) * TILE


def px(p):
    return ((p[0] - X0) / MPP, (Y1 - p[1]) / MPP)


TCOL = {0: (150, 156, 128), 1: (112, 162, 82), 2: (62, 112, 56), 3: (156, 150, 96), 4: (178, 174, 164), 5: (222, 210, 160),
        6: (98, 140, 72), 7: (118, 150, 102), 8: (122, 112, 104), 9: (140, 120, 120)}
for (tx, ty) in tiles:
    x0, y0 = tx * TILE, ty * TILE
    dr.rectangle([px((x0, y0 + TILE)), px((x0 + TILE, y0))], fill=(140, 150, 122))
for (tx, ty), d in tiles.items():
    for code, ring in d["t"]:
        dr.polygon([px(p) for p in ring], fill=TCOL.get(code, TCOL[0]))
for (tx, ty), d in tiles.items():
    for code, ring in d["w"]:
        dr.polygon([px(p) for p in ring], fill=(70, 120, 175))
RCOL = {0: (250, 250, 245), 1: (200, 110, 95), 2: (215, 205, 190), 3: (250, 210, 120)}
for (tx, ty), d in tiles.items():
    for p, line in d["r"]:
        k = road_kind(p)
        dr.line([px(q) for q in line], fill=RCOL[k], width=max(1, int(road_width(p) / MPP + (1 if k in (0, 3) else 0))))
for (tx, ty), d in tiles.items():
    for h, ring in d["b"]:
        dr.polygon([px(p) for p in ring], fill=(96, 86, 92))
img.save(OUT_PNG, optimize=True)
with open(OUT_META, "w") as f:
    f.write("%f %f %f %d %d\n" % (X0, Y1, MPP, W, H))
print("kaart: %s %dx%d, %.1f MB" % (OUT_PNG, W, H, os.path.getsize(OUT_PNG) / 1048576))
