# Draws the Loguia mark and writes every logo file:
#   python3 assets/make_logo.py          the SVGs
#   python3 assets/make_logo.py --png    and the PNGs (needs rsvg-convert)
# The wordmark is JetBrains Mono Bold (the font inside loguia.h), turned into outlines so it
# looks the same everywhere: pass the .ttf with --font, or let fc-match find it.
# The mark: a window whose docked panes form an L, beside the free space and a slider.
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
INK, PANEL, BAR, ACCENT, SOFT, TEXT, DIM, WHITE = "#1B1E26", "#262C3A", "#34405A", "#5B9DFF", "#3A4A6E", "#E6E9EF", "#8B93A5", "#FFFFFF"


def rr(x, y, w, h, r, fill):
    return f'  <rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}"/>\n'


def mark(ox=0, oy=0, s=1.0):
    """the icon, 512 x 512, moved to (ox, oy) and scaled by s"""
    b = f'<g transform="translate({ox} {oy}) scale({s})">\n'
    b += rr(24, 24, 464, 464, 104, INK)
    b += rr(88, 96, 336, 44, 14, BAR)                                   # the title bar
    b += f'  <circle cx="118" cy="118" r="9" fill="{ACCENT}"/>\n'
    b += rr(140, 112, 96, 12, 6, DIM)                                   # its title
    # the L: a pane docked on the left and one along the bottom, as one shape with round corners
    b += (f'  <path d="M102 160 H186 Q200 160 200 174 V298 Q200 312 214 312 H410 Q424 312 424 326 V402 '
          f'Q424 416 410 416 H102 Q88 416 88 402 V174 Q88 160 102 160 Z" fill="{ACCENT}"/>\n')
    b += rr(220, 160, 204, 132, 14, PANEL)                              # the free space, with a slider in it
    b += rr(244, 186, 120, 12, 6, DIM)
    b += rr(244, 236, 156, 28, 10, SOFT)
    b += rr(244, 236, 92, 28, 10, ACCENT)
    b += rr(328, 230, 14, 40, 7, WHITE)
    b += rr(112, 344, 88, 12, 6, WHITE)                                 # text on the bottom pane
    b += rr(112, 372, 148, 12, 6, WHITE + "B3")
    return b + "</g>\n"


def outlines(text, font, size):
    """text as one SVG path, baseline at y = 0, and its width"""
    from fontTools.ttLib import TTFont
    from fontTools.pens.svgPathPen import SVGPathPen
    from fontTools.pens.transformPen import TransformPen
    f = TTFont(font)
    gs, cmap, k = f.getGlyphSet(), f.getBestCmap(), size / f["head"].unitsPerEm
    d, x = "", 0.0
    for ch in text:
        name = cmap[ord(ch)]
        pen = SVGPathPen(gs)
        gs[name].draw(TransformPen(pen, (k, 0, 0, -k, x, 0)))
        d += pen.getCommands()
        x += gs[name].width * k
    return d, x


def doc(w, h, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">\n'
            f'  <title>{title}</title>\n{body}</svg>\n')


def write(name, text):
    with open(os.path.join(HERE, name), "w") as f:
        f.write(text)


def main():
    font = None
    if "--font" in sys.argv:
        font = sys.argv[sys.argv.index("--font") + 1]
    else:
        font = subprocess.run(["fc-match", "-f", "%{file}", "JetBrains Mono:bold"], capture_output=True, text=True).stdout
    word, ww = outlines("loguia", font, 132)

    write("icon.svg", doc(512, 512, mark(), "Loguia"))
    for name, color in (("logo.svg", INK), ("logo-dark.svg", TEXT)):      # the mark and the word
        W = int(176 + 30 + ww + 10)
        body = mark(0, 0, 176 / 512)
        body += f'  <path transform="translate(206 122)" d="{word}" fill="{color}"/>\n'
        write(name, doc(W, 176, body, "Loguia"))
    if "--png" in sys.argv:
        for svg, png, w in (("icon.svg", "icon.png", 512), ("icon.svg", "icon-128.png", 128),
                            ("logo.svg", "logo.png", 920), ("logo-dark.svg", "logo-dark.png", 920)):
            subprocess.run(["rsvg-convert", "-w", str(w), "-o", os.path.join(HERE, png), os.path.join(HERE, svg)], check=True)


main()
