#!/usr/bin/env python3
"""Generate 320x240 pause-slide logo concepts for DABstar."""

import math
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 320, 240
S = 4                       # supersample factor
CW, CH = W * S, H * S

FONT_BOLD = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"
OUT = os.path.dirname(os.path.abspath(__file__))

NAVY_TOP = (5, 14, 34)
NAVY_BOT = (12, 42, 84)
CYAN = (79, 195, 247)
CYAN_HI = (150, 226, 255)
BLUE = (30, 123, 214)
WHITE = (255, 255, 255)
AMBER = (255, 176, 46)


# ---------------------------------------------------------------- helpers
def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def vgradient(size, top, bot):
    w, h = size
    img = Image.new("RGB", (1, h))
    px = img.load()
    for y in range(h):
        px[0, y] = lerp(top, bot, y / max(1, h - 1))
    return img.resize((w, h), Image.BILINEAR)


def radial_mask(size, cx, cy, r, power=1.0):
    """White in the centre, fading to black at radius r."""
    w, h = size
    small = max(8, int(r / 4))
    m = Image.new("L", (small * 2, small * 2), 0)
    d = ImageDraw.Draw(m)
    steps = 48
    for i in range(steps, 0, -1):
        t = i / steps
        rr = small * t
        v = int(255 * ((1 - t) ** power))
        d.ellipse([small - rr, small - rr, small + rr, small + rr], fill=v)
    m = m.resize((int(r * 2), int(r * 2)), Image.BICUBIC)
    out = Image.new("L", (w, h), 0)
    out.paste(m, (int(cx - r), int(cy - r)))
    return out


def star_points(cx, cy, r_out, r_in, n=5, rot=-90):
    pts = []
    for i in range(n * 2):
        ang = math.radians(rot + i * 180.0 / n)
        r = r_out if i % 2 == 0 else r_in
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    return pts


def add_glow(base, shape_layer, colour, blur, strength=1.0):
    """shape_layer: L mask of what should glow."""
    g = shape_layer.filter(ImageFilter.GaussianBlur(blur))
    if strength != 1.0:
        g = g.point(lambda v: min(255, int(v * strength)))
    base.paste(Image.new("RGB", base.size, colour), (0, 0), g)


def arc_ring(draw, cx, cy, radius, width, start, end, fill):
    bb = [cx - radius, cy - radius, cx + radius, cy + radius]
    draw.arc(bb, start, end, fill=fill, width=width)


def text_size(font, s):
    bb = font.getbbox(s)
    return bb[2] - bb[0], bb[3] - bb[1], bb[0], bb[1]


def draw_wordmark(img, cy, size, gap_scale=1.0, c1=WHITE, c2=CYAN, tracking=0):
    """Centred 'DABstar' with two colours. Returns (left, right, top, bottom)."""
    f = ImageFont.truetype(FONT_BOLD, size)
    d = ImageDraw.Draw(img)

    def adv(s):
        return d.textlength(s, font=f) + tracking * len(s)

    w1, w2 = adv("DAB"), adv("star")
    total = w1 + w2 * gap_scale
    x = (CW - total) / 2
    top = cy - size * 0.5
    for s, col in (("DAB", c1), ("star", c2)):
        for ch in s:
            d.text((x, cy), ch, font=f, fill=col, anchor="lm")
            x += d.textlength(ch, font=f) + tracking
    return (CW - total) / 2, (CW + total) / 2, top, cy + size * 0.5


def caption(img, cx, y, txt, size, colour, tracking):
    f = ImageFont.truetype(FONT_REG, size)
    d = ImageDraw.Draw(img)
    total = sum(d.textlength(c, font=f) + tracking for c in txt) - tracking
    x = cx - total / 2
    for c in txt:
        d.text((x, y), c, font=f, fill=colour, anchor="lm")
        x += d.textlength(c, font=f) + tracking


def vignette(img, strength=0.55):
    w, h = img.size
    m = radial_mask((w, h), w / 2, h / 2, max(w, h) * 0.78, power=0.55)
    m = m.point(lambda v: int((255 - v) * strength))
    img.paste(Image.new("RGB", (w, h), (0, 0, 0)), (0, 0), m)


def finish(img, name):
    out = img.resize((W, H), Image.LANCZOS)
    path = os.path.join(OUT, name)
    out.save(path)
    print(path)
    return out


# ---------------------------------------------------------------- variant A
def variant_a():
    """Signal: star + wave arcs, echoing the app icon."""
    img = vgradient((CW, CH), NAVY_TOP, NAVY_BOT)

    glow = Image.new("L", (CW, CH), 0)
    gd = ImageDraw.Draw(glow)
    layer = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # three wave arcs sweeping right from the star
    ax, ay = CW * 0.335, CH * 0.365
    for i, (rad, wdt, col, span) in enumerate((
            (CH * 0.205, int(CH * 0.045), CYAN_HI, 52),
            (CH * 0.310, int(CH * 0.042), CYAN, 48),
            (CH * 0.415, int(CH * 0.038), (60, 150, 220), 44),
    )):
        arc_ring(d, ax, ay, rad, wdt, -span, span, col + (255,))
        arc_ring(gd, ax, ay, rad, wdt, -span, span, 190 - i * 45)

    # star, sitting on the centre of curvature of the arcs
    sr = CH * 0.150
    pts = star_points(ax - CH * 0.020, ay, sr, sr * 0.40)
    d.polygon(pts, fill=WHITE + (255,))
    gd.polygon(pts, fill=255)

    # centre the whole symbol group horizontally on the canvas
    bb = layer.getbbox()
    tf = (1, 0, -int(round(CW / 2 - (bb[0] + bb[2]) / 2)), 0, 1, 0)
    layer = layer.transform(layer.size, Image.AFFINE, tf, resample=Image.BICUBIC)
    glow = glow.transform(glow.size, Image.AFFINE, tf, resample=Image.BICUBIC)

    # soft cyan bloom behind the emitter
    bloom = radial_mask((CW, CH), CW / 2, CH * 0.36, CH * 0.62, power=1.5)
    bloom = bloom.point(lambda v: int(v * 0.30))
    img.paste(Image.new("RGB", (CW, CH), (26, 96, 168)), (0, 0), bloom)

    add_glow(img, glow, (90, 200, 255), CH * 0.045, 0.55)
    img.paste(layer, (0, 0), layer)

    draw_wordmark(img, CH * 0.764, int(CH * 0.170), tracking=-CH * 0.004)
    caption(img, CW / 2, CH * 0.905, "DIGITAL AUDIO BROADCASTING",
            int(CH * 0.048), (128, 170, 215), CH * 0.014)

    vignette(img, 0.5)
    return finish(img, "variant_a.png")


# ---------------------------------------------------------------- variant B
def variant_b():
    """Spectrum: OFDM carrier bars under the wordmark."""
    img = vgradient((CW, CH), (4, 12, 30), (10, 34, 70))

    glow = Image.new("L", (CW, CH), 0)
    gd = ImageDraw.Draw(glow)
    layer = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # spectrum bars, envelope shaped like a DAB block
    n = 33
    x0, x1 = CW * 0.085, CW * 0.915
    base_y = CH * 0.615
    slot = (x1 - x0) / n
    bw = slot * 0.55
    heights = []
    for i in range(n):
        t = i / (n - 1)
        env = math.sin(math.pi * t) ** 0.42
        ripple = 0.72 + 0.28 * abs(math.sin(t * 11.0 + 0.6)) * abs(math.cos(t * 4.3))
        heights.append(env * ripple)
    for i, hn in enumerate(heights):
        bh = CH * 0.30 * hn
        x = x0 + i * slot + (slot - bw) / 2
        col = lerp(BLUE, CYAN_HI, hn)
        d.rounded_rectangle([x, base_y - bh, x + bw, base_y],
                            radius=bw * 0.35, fill=col + (255,))
        gd.rounded_rectangle([x, base_y - bh, x + bw, base_y],
                             radius=bw * 0.35, fill=int(90 + 110 * hn))

    # star riding the peak
    sr = CH * 0.115
    sx, sy = CW * 0.5, base_y - CH * 0.30 - sr * 0.85
    pts = star_points(sx, sy, sr, sr * 0.40)
    d.polygon(pts, fill=WHITE + (255,))
    gd.polygon(pts, fill=255)

    add_glow(img, glow, (70, 175, 245), CH * 0.05, 0.6)
    img.paste(layer, (0, 0), layer)

    # baseline
    dd = ImageDraw.Draw(img)
    dd.rectangle([x0, base_y, x1, base_y + CH * 0.008], fill=(46, 106, 170))

    draw_wordmark(img, CH * 0.775, int(CH * 0.185), tracking=-CH * 0.004)
    caption(img, CW / 2, CH * 0.905, "DIGITAL AUDIO BROADCASTING",
            int(CH * 0.048), (120, 162, 210), CH * 0.014)

    vignette(img, 0.45)
    return finish(img, "variant_b.png")


# ---------------------------------------------------------------- variant C
def variant_c():
    """Emblem: circular badge with radiating waves."""
    img = vgradient((CW, CH), (6, 16, 38), (13, 45, 88))

    cx, cy = CW * 0.5, CH * 0.40
    bloom = radial_mask((CW, CH), cx, cy, CH * 0.55, power=1.6)
    bloom = bloom.point(lambda v: int(v * 0.34))
    img.paste(Image.new("RGB", (CW, CH), (28, 104, 180)), (0, 0), bloom)

    glow = Image.new("L", (CW, CH), 0)
    gd = ImageDraw.Draw(glow)
    layer = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # symmetric wave pairs left and right of the star
    for i, (rad, wdt, col) in enumerate((
            (CH * 0.175, int(CH * 0.040), CYAN_HI),
            (CH * 0.265, int(CH * 0.036), CYAN),
            (CH * 0.355, int(CH * 0.032), (56, 144, 216)),
    )):
        span = 46 - i * 4
        for base in (0, 180):
            arc_ring(d, cx, cy, rad, wdt, base - span, base + span, col + (255,))
            arc_ring(gd, cx, cy, rad, wdt, base - span, base + span, 180 - i * 40)

    # outer ring
    r = CH * 0.425
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(56, 132, 200, 210),
              width=int(CH * 0.010))

    sr = CH * 0.135
    pts = star_points(cx, cy, sr, sr * 0.40)
    d.polygon(pts, fill=WHITE + (255,))
    gd.polygon(pts, fill=255)

    add_glow(img, glow, (86, 196, 255), CH * 0.045, 0.55)
    img.paste(layer, (0, 0), layer)

    draw_wordmark(img, CH * 0.845, int(CH * 0.165), tracking=-CH * 0.004)
    vignette(img, 0.5)
    return finish(img, "variant_c.png")


# ---------------------------------------------------------------- contact sheet
def sheet(imgs):
    pad, lbl = 10, 16
    sh = Image.new("RGB", (len(imgs) * (W + pad) + pad, H + lbl + pad * 2), (30, 30, 34))
    d = ImageDraw.Draw(sh)
    f = ImageFont.truetype(FONT_BOLD, 12)
    for i, (name, im) in enumerate(imgs):
        x = pad + i * (W + pad)
        sh.paste(im, (x, pad))
        d.text((x + W / 2, pad + H + 3), name, font=f, fill=(210, 210, 215), anchor="mt")
    p = os.path.join(OUT, "sheet.png")
    sh.save(p)
    print(p)


if __name__ == "__main__":
    a, b, c = variant_a(), variant_b(), variant_c()
    sheet([("A - Signal", a), ("B - Spectrum", b), ("C - Emblem", c)])
