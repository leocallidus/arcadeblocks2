"""
Generate the few missing sprites for boss 3:
- boss3_laser_warning.png   -> thin red vertical translucent strip (charging telegraph)
- boss3_laser_beam.png      -> bright red+white vertical beam (firing)
All sprites are placed DIRECTLY in assets/sprites/ (they are also added to the atlas
by a separate add_boss3_to_atlas.py step).
"""
from PIL import Image
import os
import math

OUT = "/home/leo/arcadeblocks-src/arcadeblocks2/assets/sprites"
os.makedirs(OUT, exist_ok=True)


def vertical_laser(width: int, height: int, core_white: bool) -> Image.Image:
    """
    Build a vertical laser strip, alpha-correct for additive feel.
    core_white=True -> bright hot beam (firing); otherwise faint warning telegraph.
    """
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    pixels = img.load()
    cx = (width - 1) / 2.0
    half_w = width / 2.0

    for y in range(height):
        # tiny vertical falloff so the beam is hottest in the middle 60% of its length
        dy_norm = abs((y - height / 2.0)) / (height / 2.0)
        v_factor = max(0.0, 1.0 - dy_norm * 0.4)

        for x in range(width):
            dx = abs(x - cx)
            t = dx / half_w  # 0 at center, 1 at edge

            # Horizontal alpha (radial-like)
            a = (1.0 - t) ** 2
            a = a * v_factor

            if core_white:
                r = 255
                g = int(140 + 60 * (1.0 - t))
                b = int(140 + 60 * (1.0 - t))
                # White-hot inner core in the middle column
                if t < 0.18:
                    r, g, b = 255, 250, 240
            else:
                # Warning telegraph: shift towards lighter pink at edges so it reads as
                # 'alarm' rather than 'laser fire'.
                r = int(220 + 35 * t)
                g = int(40 + 60 * t)
                b = int(80 + 90 * t)

            alpha = int(min(255, max(0, a * 255)))
            if alpha > 0:
                pixels[x, y] = (r, g, b, alpha)
    return img


# Two beam thicknesses so phase-1 and phase-2 beams differ.
# Width matches boss.laserWidth in the gameplay code.
def make_laser_pair(label_prefix: str, width: int):
    name_w = os.path.join(OUT, f"{label_prefix}_laser_warning.png")
    name_b = os.path.join(OUT, f"{label_prefix}_laser_beam.png")
    img_w = vertical_laser(width, 800, core_white=False)
    img_b = vertical_laser(width, 800, core_white=True)
    img_w.save(name_w, "PNG")
    img_b.save(name_b, "PNG")
    print(f"  wrote {name_w}  ({img_w.size})")
    print(f"  wrote {name_b}  ({img_b.size})")
    return name_w, name_b


def make_teleport_flash() -> str:
    size = 200
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = img.load()
    cx = cy = (size - 1) / 2.0

    # Three concentric rings + slightly noisy particles
    rings = [
        (size * 0.20, 3.0, (160, 220, 255)),
        (size * 0.36, 2.0, (120, 200, 255)),
        (size * 0.50, 1.5, (90, 180, 250)),
    ]
    for radius, thickness, color in rings:
        r_in = radius - thickness
        r_out = radius + thickness
        for y in range(size):
            for x in range(size):
                dx = x - cx
                dy = y - cy
                dist = math.hypot(dx, dy)
                if r_in <= dist <= r_out:
                    a = 1.0 - abs(dist - radius) / thickness
                    alpha = int(a * 220)
                    pixels[x, y] = (color[0], color[1], color[2], alpha)

    # A bright center disc
    for y in range(size):
        for x in range(size):
            dx = x - cx
            dy = y - cy
            dist = math.hypot(dx, dy)
            if dist < size * 0.10:
                a = 1.0 - dist / (size * 0.10)
                alpha = int(a * 200)
                pixels[x, y] = (220, 240, 255, alpha)

    # Sparse data motes
    motes = [
        (0.42, 0.18, 3),
        (0.55, 0.30, 2),
        (0.62, -0.10, 2),
        (-0.33, 0.45, 4),
        (-0.50, -0.20, 3),
        (0.28, -0.48, 2),
    ]
    for (mx, my, r) in motes:
        px = int(cx + mx * size)
        py = int(cy + my * size)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                xx, yy = px + dx, py + dy
                if 0 <= xx < size and 0 <= yy < size:
                    if dx * dx + dy * dy <= r * r:
                        a = 1.0 - math.hypot(dx, dy) / r
                        alpha = int(a * 180)
                        pixels[xx, yy] = (160, 220, 255, alpha)

    name = os.path.join(OUT, "boss3_teleport_flash.png")
    img.save(name, "PNG")
    print(f"  wrote {name}  ({img.size})")
    return name


def main():
    print("Generating boss3 laser/teleport sprites...")
    # Phase 1 (warning=thin, beam=thin)
    make_laser_pair("boss3", width=32)
    # Phase 2 (wider beams, separate filenames for runtime flexibility)
    make_laser_pair("boss3_wide", width=48)
    make_teleport_flash()
    print("Done.")


if __name__ == "__main__":
    main()
