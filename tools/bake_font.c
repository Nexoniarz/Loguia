/* bake_font: turns a .ttf into the font block embedded in loguia.h.
 *
 *   cc -O2 -I<dir with stb_truetype.h> tools/bake_font.c -lm -o bake_font
 *   ./bake_font font.ttf 15 loguia.h "Font name, licence"
 *
 * It rewrites everything between LG_FONT_BEGIN and LG_FONT_END in loguia.h, and the same
 * font at twice the size (for HiDPI screens) between LG_FONT2X_BEGIN and LG_FONT2X_END.
 * Only needed when you want a different font or size; using Loguia never needs it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#ifdef USE_IMSTB
#include "imstb_truetype.h"
#else
#include "stb_truetype.h"
#endif


typedef struct { int cp, x, y, w, h, xo, yo, adv; unsigned char *bmp; } Glyph;

/* must match lg__font_alphabet in loguia.h: printable ASCII without  "  \  ?  */
static const char ALPHABET[] =
    "!#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz";

static int by_height(const void *a, const void *b) {
    const Glyph *x = a, *y = b;
    if (y->h != x->h) return y->h - x->h;
    return x->cp - y->cp;
}
static int by_cp(const void *a, const void *b) { return ((const Glyph *)a)->cp - ((const Glyph *)b)->cp; }

/* Rasterises the font at one size and writes it as C between "LG_<tag>_BEGIN" and "LG_<tag>_END".
   n: "" for the normal block, "2" for the double-size one (it ends up in the names). */
static int bake(stbtt_fontinfo *font, float size, int ATLAS_W, const char *tag, const char *n, const char *path, const char *credit) {
    float scale = stbtt_ScaleForMappingEmToPixels(font, size);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(font, &asc, &desc, &gap);
    int ascent = (int)floorf(asc * scale + 0.5f);
    int line_h = (int)ceilf((asc - desc + gap) * scale);

    /* the characters Loguia can draw */
    int cps[1024], ncp = 0;
    for (int c = 32; c <= 126; c++) cps[ncp++] = c;
    for (int c = 160; c <= 383; c++) cps[ncp++] = c;      /* Latin-1 and Latin Extended-A */
    static const int extra[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026,
                                 0x20AC, 0x2190, 0x2192, 0x25B6, 0x25BC, 0x2713 };
    for (size_t i = 0; i < sizeof extra / sizeof *extra; i++) cps[ncp++] = extra[i];

    Glyph *g = calloc(ncp, sizeof *g);
    int ng = 0;
    for (int i = 0; i < ncp; i++) {
        int gi = stbtt_FindGlyphIndex(font, cps[i]);
        if (gi == 0) continue;
        Glyph *q = &g[ng++];
        int adv, lsb;
        stbtt_GetGlyphHMetrics(font, gi, &adv, &lsb);
        q->cp = cps[i];
        q->adv = (int)floorf(adv * scale + 0.5f);
        q->bmp = stbtt_GetGlyphBitmap(font, scale, scale, gi, &q->w, &q->h, &q->xo, &q->yo);
        q->yo += ascent;                                    /* offset from the top of the line */
        if (!q->bmp) q->w = q->h = 0;
    }

    /* shelf packing; a 2x2 white block sits at (0,0) for solid fills */
    qsort(g, ng, sizeof *g, by_height);
    int x = 3, y = 0, shelf = 2;
    for (int i = 0; i < ng; i++) {
        if (!g[i].w) continue;
        if (x + g[i].w + 1 > ATLAS_W) { x = 0; y += shelf + 1; shelf = 0; }
        g[i].x = x; g[i].y = y;
        x += g[i].w + 1;
        if (g[i].h > shelf) shelf = g[i].h;
    }
    int atlas_h = y + shelf;
    unsigned char *atlas = calloc(ATLAS_W, atlas_h);
    atlas[0] = atlas[1] = atlas[ATLAS_W] = atlas[ATLAS_W + 1] = 255;
    for (int i = 0; i < ng; i++)
        for (int r = 0; r < g[i].h; r++)
            memcpy(atlas + (g[i].y + r) * ATLAS_W + g[i].x, g[i].bmp + r * g[i].w, g[i].w);
    qsort(g, ng, sizeof *g, by_cp);

    /* 4 bits per pixel, run-length coded: codes 0..79 are (value, run 1..5), 80..84 are long runs of zero */
    static const int zrun[] = { 10, 20, 40, 80, 160 };
    int total = ATLAS_W * atlas_h, len = 0;
    char *enc = malloc(total + 1);
    for (int i = 0; i < total;) {
        int v = (atlas[i] + 8) / 17, run = 1;
        while (i + run < total && (atlas[i + run] + 8) / 17 == v) run++;
        i += run;
        while (run > 0) {
            int z = -1;
            if (v == 0) for (int k = 4; k >= 0; k--) if (run >= zrun[k]) { z = k; break; }
            if (z >= 0) { enc[len++] = ALPHABET[80 + z]; run -= zrun[z]; }
            else { int r = run > 5 ? 5 : run; enc[len++] = ALPHABET[v * 5 + r - 1]; run -= r; }
        }
    }

    /* splice the block into loguia.h */
    char begin[64], end[64];
    snprintf(begin, sizeof begin, "/* LG_%s_BEGIN", tag);
    snprintf(end, sizeof end, "/* LG_%s_END */", tag);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END); long size_of = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc(size_of + 1);
    if (fread(src, 1, size_of, f) != (size_t)size_of) return 1;
    src[size_of] = 0; fclose(f);
    char *b = strstr(src, begin), *e = strstr(src, end);
    if (!b || !e) { fprintf(stderr, "no %s / %s markers in %s\n", begin, end, path); return 1; }

    f = fopen(path, "wb");
    fwrite(src, 1, b - src, f);
    fprintf(f, "%s: made by tools/bake_font.c, do not edit by hand.\n   Font: %s, %g px. */\n", begin, credit, size);
    fprintf(f, "#define LG__ATLAS%s_W %d\n#define LG__ATLAS%s_H %d\n#define LG__FONT%s_LINE %d\n#define LG__FONT%s_GLYPHS %d\n",
            n, ATLAS_W, n, atlas_h, n, line_h, n, ng);
    fprintf(f, "static const lg_glyph lg__glyphs%s[LG__FONT%s_GLYPHS] = {\n", n, n);
    for (int i = 0; i < ng; i++)
        fprintf(f, "{%d,%d,%d,%d,%d,%d,%d,%d},%s", g[i].cp, g[i].x, g[i].y, g[i].w, g[i].h, g[i].xo, g[i].yo, g[i].adv,
                i % 6 == 5 ? "\n" : "");
    fprintf(f, "\n};\nstatic const char *const lg__font%s_rle[] = {\n", n);
    for (int i = 0; i < len; i += 120) fprintf(f, "\"%.*s\",\n", len - i < 120 ? len - i : 120, enc + i);
    fprintf(f, "0 };\n");
    fwrite(e, 1, strlen(e), f);
    fclose(f);
    fprintf(stderr, "%g px: %d glyphs, atlas %dx%d, line height %d, %d bytes of font data\n", size, ng, ATLAS_W, atlas_h, line_h, len);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: bake_font font.ttf size loguia.h \"font credit\"\n"); return 1; }
    float size = (float)atof(argv[2]);

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *ttf = malloc(n);
    if (fread(ttf, 1, n, f) != (size_t)n) return 1;
    fclose(f);

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) { fprintf(stderr, "bad font\n"); return 1; }
    return bake(&font, size, 256, "FONT", "", argv[3], argv[4]) || bake(&font, size * 2, 512, "FONT2X", "2", argv[3], argv[4]);
}
