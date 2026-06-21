"""
Generate boss 4 ("Singularity") sprites programmatically with PIL.

Outputs go straight into assets/sprites/ so they are picked up by the
renderer the same way the boss 3 sprites are. Visual design is intentionally
procedural so we can iterate cheaply from BIOS without waiting on Midjourney.
"""

from PIL import Image, ImageDraw, ImageFilter
import math
import os
import random

OUT_DIR = "/home/leo/arcadeblocks-src/arcadeblocks2/assets/sprites"
os.makedirs(OUT_DIR, exist_ok=True)


def _radial(size: int, fn):
    """Return RGBA image where the alpha at (x, y) follows fn(r/half, angle)."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = cy = (size - 1) * 0.5
    half = cx
    pixels = img.load()
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx, y - cy
            r = math.hypot(dx, dy) / half
            ang = math.atan2(dy, dx)
            r_ch, g_ch, b_ch, a_ch = fn(r, ang, dx, dy)
            pixels[x, y] = (r_ch, g_ch, b_ch, a_ch)
    return img


def core_sprite():
    size = 300
    # Dark purple base background.
    base = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    draw = ImageDraw.Draw(base)
    cx = cy = (size - 1) * 0.5
    # Outer dark purple nebula
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - cx, y - cy) / cx
            if d > 1.0:
                continue
            v = 1.0 - d
            v = max(v, 0)
            base.putpixel((x, y), (40 + int(40 * v), 18 + int(20 * v), 70 + int(60 * v), int(255 * v * v)))
    # Layered concentric rings of glow
    cx_i = int(cx)
    cy_i = int(cy)
    for r_a, r_b, col in [
        (140, 150, (60, 30, 110, 220)),
        (110, 140, (90, 50, 160, 200)),
        (80, 110, (140, 80, 200, 220)),
        (50, 80, (190, 130, 230, 230)),
        (25, 50, (235, 200, 245, 240)),
        (10, 25, (255, 230, 200, 250)),
        (0, 10, (255, 250, 230, 255)),
    ]:
        draw.ellipse(
            [cx_i - r_b, cy_i - r_b, cx_i + r_b, cy_i + r_b],
            fill=col,
            outline=None,
        )
    # Decorative orbiting sparks (subtle for a 2D game).
    rng = random.Random(42)
    for _ in range(80):
        ang = rng.uniform(0, 2 * math.pi)
        rad = rng.uniform(40, 138)
        x = cx + math.cos(ang) * rad
        y = cy + math.sin(ang) * rad
        a = max(0, 220 - int(rad * 1.0))
        sz = rng.choice([1, 1, 2])
        draw.ellipse([x - sz, y - sz, x + sz, y + sz], fill=(255, 220, 255, a))
    base = base.filter(ImageFilter.GaussianBlur(radius=0.6))
    base.save(os.path.join(OUT_DIR, "boss4_core.png"))


def accretion_sprite():
    """Outer dark accretion disk with hot-spots embedded."""
    size = 320
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = (size - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)

    # Outer dark disk (filled annulus).
    draw.ellipse(
        [cx_i - 150, cy_i - 90, cx_i + 150, cy_i + 90],
        fill=(40, 16, 70, 255),
    )
    # Inner darker core region (where the singularity will paint over).
    draw.ellipse(
        [cx_i - 110, cy_i - 66, cx_i + 110, cy_i + 66],
        fill=(20, 6, 40, 255),
    )
    # Gradient rings on the disk (accent + weather).
    for r_outer, r_inner, col in [
        (150, 144, (95, 80, 165, 255)),
        (138, 132, (130, 90, 200, 255)),
        (122, 116, (170, 110, 220, 255)),
    ]:
        row_h = int(r_outer * 0.6)
        draw.ellipse(
            [cx_i - r_outer, cy_i - row_h,
             cx_i + r_outer, cy_i + row_h],
            outline=col,
            width=r_outer - r_inner,
        )
    # Bright streaks on the disk.
    rng = random.Random(7)
    for _ in range(120):
        ang = rng.uniform(0, 2 * math.pi)
        rad = rng.uniform(120, 148)
        sx = cx + math.cos(ang) * rad
        sy = cy + math.sin(ang) * rad * 0.6
        a = rng.randint(100, 230)
        sz = rng.choice([2, 2, 3])
        col_h = rng.choice([(255, 220, 180), (255, 170, 130), (255, 240, 220)])
        draw.ellipse([sx - sz, sy - sz, sx + sz, sy + sz], fill=(*col_h, a))
    img = img.filter(ImageFilter.GaussianBlur(radius=1.0))
    img.save(os.path.join(OUT_DIR, "boss4_accretion.png"))


def hotspot_sprite():
    size = 32
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = (size - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)
    # Soft outer halo
    for r, alpha in [
        (15, 64), (12, 110), (9, 170), (6, 220), (3, 255),
    ]:
        col = (255, 240, 200, alpha)
        draw.ellipse([cx_i - r, cy_i - r, cx_i + r, cy_i + r], fill=col)
    img = img.filter(ImageFilter.GaussianBlur(radius=0.6))
    img.save(os.path.join(OUT_DIR, "boss4_hotspot.png"))


def drone_sprite():
    """Tiny hexagonal dark drone with 4 red eyes. Same as boss3 look but with
    a violet frame so the two bosses read as visually distinct."""
    size = 34
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = (size - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)
    # Hex frame
    pts = []
    for ang in range(0, 360, 60):
        rad = 14
        pts.append((cx_i + math.cos(math.radians(ang)) * rad,
                    cy_i + math.sin(math.radians(ang)) * rad * 0.85))
    draw.polygon(pts, fill=(40, 14, 70, 255), outline=(150, 110, 220, 255))
    # Centre dark dot
    draw.ellipse([cx_i - 5, cy_i - 5, cx_i + 5, cy_i + 5], fill=(8, 0, 20, 255))
    # 4 red eyes at corners
    eye_offsets = [(-7, -4), (7, -4), (-7, 4), (7, 4)]
    for ox, oy in eye_offsets:
        draw.ellipse(
            [cx_i + ox - 2, cy_i + oy - 2, cx_i + ox + 2, cy_i + oy + 2],
            fill=(255, 80, 80, 255),
        )
    img = img.filter(ImageFilter.GaussianBlur(radius=0.4))
    img.save(os.path.join(OUT_DIR, "boss4_drone.png"))


def mine_sprite():
    size = 60
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = (size - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)
    # Dark core
    draw.ellipse([cx_i - 14, cy_i - 14, cx_i + 14, cy_i + 14],
                 fill=(8, 0, 0, 255))
    # Red pulsing ring
    for r, alpha in [
        (28, 50), (24, 100), (20, 160), (16, 220),
    ]:
        col = (255, 60, 60, alpha)
        draw.ellipse([cx_i - r, cy_i - r, cx_i + r, cy_i + r], outline=col, width=1)
    img = img.filter(ImageFilter.GaussianBlur(radius=0.6))
    img.save(os.path.join(OUT_DIR, "boss4_mine.png"))


def pulse_sprite():
    size = 800
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = (size - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)
    # Three nested rings with smooth alpha gradient.
    for r, color in [
        (380, (200, 100, 255, 180)),
        (370, (200, 100, 255, 130)),
        (350, (240, 160, 255, 90)),
        (320, (240, 160, 255, 50)),
    ]:
        draw.ellipse(
            [cx_i - r, cy_i - r, cx_i + r, cy_i + r],
            outline=color,
            width=4,
        )
    img = img.filter(ImageFilter.GaussianBlur(radius=1.2))
    img.save(os.path.join(OUT_DIR, "boss4_pulse.png"))


def destroyed_sprite():
    """Same layout as `core_sprite` but with a layer of fractured cracks
    drawn over a desaturated core."""
    img = core_sprite_mark()
    draw = ImageDraw.Draw(img)
    cx = cy = (img.size[0] - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)
    rng = random.Random(99)
    # Radial cracks
    for _ in range(22):
        ang = rng.uniform(0, 2 * math.pi)
        # inner endpoint
        r_inner = rng.uniform(20, 60)
        sx = cx + math.cos(ang) * r_inner
        sy = cy + math.sin(ang) * r_inner
        # outer endpoint
        r_outer = rng.uniform(120, 145)
        ex = cx + math.cos(ang) * r_outer
        ey = cy + math.sin(ang) * r_outer * 0.85
        # jagged path between
        segments = rng.randint(4, 7)
        last_x, last_y = sx, sy
        for seg in range(1, segments + 1):
            t = seg / segments
            tx = sx + (ex - sx) * t
            ty = sy + (ey - sy) * t + rng.uniform(-4, 4)
            draw.line([(last_x, last_y), (tx, ty)], fill=(255, 220, 200, 230), width=2)
            last_x, last_y = tx, ty
    # Dim the whole image by overlaying a transparent dark wash.
    overlay = Image.new("RGBA", img.size, (10, 0, 25, 100))
    img.alpha_composite(overlay)
    img.save(os.path.join(OUT_DIR, "boss4_destroyed.png"))


def core_sprite_mark():
    """Re-render the core as a non-blurred base for the destroyed layer so we
    can overlay cracks without re-blurring."""
    size = 300
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = cy = (size - 1) * 0.5
    cx_i, cy_i = int(cx), int(cy)
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - cx_i, y - cy_i) / cx_i
            if d > 1:
                continue
            v = 1 - d
            img.putpixel((x, y), (60 + int(60 * v), 30 + int(40 * v), 100 + int(80 * v),
                                     int(180 * v * v)))
    return img


def main():
    print("Generating boss 4 (Singularity) sprites...")
    core_sprite()
    accretion_sprite()
    hotspot_sprite()
    drone_sprite()
    mine_sprite()
    pulse_sprite()
    destroyed_sprite()
    out = sorted(os.listdir(OUT_DIR))
    for fn in out:
        if fn.startswith("boss4_"):
            sz = os.path.getsize(os.path.join(OUT_DIR, fn))
            print(f"  wrote boss4_*.png  ({fn}: {sz} bytes)")


if __name__ == "__main__":
    main()
