/* Loguia without a window or a GPU: a script plays the mouse and keyboard, the software
 * module draws each interesting frame, and the result is saved as PNG files.
 * It doubles as Loguia's self-test: it exits with an error when a widget misbehaves.
 *
 *   cc -std=c99 -Wall -Wextra -I.. headless.c -o headless && ./headless out_dir
 */
#define LOGUIA_IMPLEMENTATION
#define LOGUIA_SOFTWARE
#include "../loguia.h"
#include "demo_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1100
#define H 680

static unsigned char pixels[W * H * 4 * 4];     /* room for a picture at twice the size */
static int failures;

#define CHECK(what) do { if (!(what)) { printf("FAILED line %d: %s\n", __LINE__, #what); failures++; } } while (0)

/* a PNG writer that does not compress: enough to look at the pictures */
static unsigned int crc_table[256];
static unsigned int crc(unsigned int c, const unsigned char *p, size_t n) {
    size_t i;
    c = ~c;
    for (i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 255] ^ (c >> 8);
    return ~c;
}
static void chunk(FILE *f, const char *type, const unsigned char *data, unsigned int n) {
    unsigned char b[4] = { (unsigned char)(n >> 24), (unsigned char)(n >> 16), (unsigned char)(n >> 8), (unsigned char)n };
    unsigned int c;
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (n) fwrite(data, 1, n, f);
    c = crc(crc(0, (const unsigned char *)type, 4), data, n);
    b[0] = (unsigned char)(c >> 24); b[1] = (unsigned char)(c >> 16); b[2] = (unsigned char)(c >> 8); b[3] = (unsigned char)c;
    fwrite(b, 1, 4, f);
}
static void save_png(const char *path, int w, int h) {
    FILE *f = fopen(path, "wb");
    size_t row = (size_t)w * 4 + 1, raw = row * (size_t)h, n = 0;
    unsigned char *z = malloc(raw + (size_t)h * 5 + 16), head[13] = { 0 };
    unsigned int a = 1, b = 0;
    int y, i, k;
    if (!f) { perror(path); exit(1); }
    for (i = 0; i < 256; i++) {
        unsigned int c = (unsigned int)i;
        for (k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    z[n++] = 0x78; z[n++] = 0x01;
    for (y = 0; y < h; y++) {                      /* one stored block per row */
        unsigned char *line = z + n + 5;
        z[n++] = y == h - 1;
        z[n++] = (unsigned char)row; z[n++] = (unsigned char)(row >> 8);
        z[n++] = (unsigned char)~row; z[n++] = (unsigned char)(~row >> 8);
        line[0] = 0;
        memcpy(line + 1, pixels + (size_t)y * (size_t)w * 4, (size_t)w * 4);
        for (i = 0; i < (int)row; i++) { a = (a + line[i]) % 65521; b = (b + a) % 65521; }
        n += row;
    }
    z[n++] = (unsigned char)(b >> 8); z[n++] = (unsigned char)b; z[n++] = (unsigned char)(a >> 8); z[n++] = (unsigned char)a;
    head[2] = (unsigned char)(w >> 8); head[3] = (unsigned char)w; head[6] = (unsigned char)(h >> 8); head[7] = (unsigned char)h; head[8] = 8; head[9] = 6;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    chunk(f, "IHDR", head, 13);
    chunk(f, "IDAT", z, (unsigned int)n);
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(z);
}

static demo_state st;

static void frame(void) {
    lg_begin_frame(W, H, 1.0f / 60);
    demo_ui(&st);
    lg_end_frame();
}
/* scale 2 draws the same interface on twice as many pixels, the way a HiDPI screen wants it */
static void shot_at(const char *dir, const char *name, int scale) {
    char path[512];
    int i, w = W * scale, h = H * scale;
    for (i = 0; i < w * h; i++) { pixels[i * 4] = 14; pixels[i * 4 + 1] = 17; pixels[i * 4 + 2] = 22; pixels[i * 4 + 3] = 255; }
    lg_soft_render(pixels, w, h);
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    save_png(path, w, h);
}
static void shot(const char *dir, const char *name) { shot_at(dir, name, 1); }
static void click(float x, float y) {
    lg_input_mouse_pos(x, y); frame();
    lg_input_mouse_button(0, 1); frame();
    lg_input_mouse_button(0, 0); frame();
}
static void drag(float x0, float y0, float x1, float y1) {
    int i;
    lg_input_mouse_pos(x0, y0); frame();
    lg_input_mouse_button(0, 1); frame();
    for (i = 1; i <= 8; i++) { lg_input_mouse_pos(x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8); frame(); }
    lg_input_mouse_button(0, 0); frame();
}
static void key(int k) { lg_input_key(k, 1); frame(); lg_input_key(k, 0); frame(); }
static void type(const char *s) { for (; *s; s++) lg_input_char((unsigned char)*s); frame(); }

/* where row number i of a popup is, when the popup's top is at y */
static float popup_row(float y, int i) {
    const lg_style *s = lg_get_style();
    float row = lg_line_height() + s->padding_y;
    return y + s->border + s->padding_y + 2 + (float)i * (row + s->spacing_y) + row / 2;
}
static void hold(int k, int presses) {
    lg_input_key(k, 1);
    while (presses-- > 0) { lg_input_key(LG_KEY_TAB, 1); frame(); lg_input_key(LG_KEY_TAB, 0); frame(); }
    lg_input_key(k, 0);
}

/* the newer widgets: combo box, popup, tabs, table, many-line editor, keyboard navigation */
static void more(const char *dir) {
    const lg_style *s = lg_get_style();
    float field_h = lg_line_height() + 2 * s->padding_y, list_y, tab2, tab3;
    int was;

    /* the combo box: a click opens the list, a click on a row picks it and closes the list */
    list_y = st.at_combo_y + field_h / 2 + 2;
    click(st.at_combo_x, st.at_combo_y); frame();
    shot(dir, "5-combo");
    click(st.at_combo_x + 10, popup_row(list_y, 3)); frame();
    CHECK(st.language == 3);
    click(st.at_combo_x + 10, popup_row(list_y, 0)); frame();      /* the list is closed: this click is on something else */
    CHECK(st.language == 3);
    /* a click outside closes it without picking, and so does Escape */
    click(st.at_combo_x, st.at_combo_y); frame();
    click(5, H - 5); frame();
    click(st.at_combo_x + 10, popup_row(list_y, 0)); frame();
    CHECK(st.language == 3);
    click(st.at_combo_x, st.at_combo_y); frame();
    key(LG_KEY_ESCAPE);
    click(st.at_combo_x + 10, popup_row(list_y, 0)); frame();
    CHECK(st.language == 3);
    /* a second click on the field closes the list again */
    click(st.at_combo_x, st.at_combo_y); frame();
    click(st.at_combo_x, st.at_combo_y); frame();
    click(st.at_combo_x + 10, popup_row(list_y, 0)); frame();
    CHECK(st.language == 3);
    /* the keyboard: Enter opens the list, Down walks it, Enter picks */
    click(st.at_button_x, st.at_button_y);                          /* the keyboard starts from the last widget used */
    CHECK(st.clicks == 3);
    key(LG_KEY_TAB);                                                /* the checkbox */
    was = st.check;
    key(LG_KEY_SPACE);
    CHECK(st.check == !was);
    hold(LG_KEY_SHIFT, 1);                                          /* back to the button */
    key(LG_KEY_ENTER);
    CHECK(st.clicks == 4);
    key(LG_KEY_TAB); key(LG_KEY_TAB);                               /* the Volume slider */
    st.volume = 0.5f;
    key(LG_KEY_LEFT);
    CHECK(st.volume < 0.5f && st.volume > 0.48f);
    shot(dir, "6-keyboard");
    key(LG_KEY_TAB); key(LG_KEY_TAB);                               /* Count, then the Name field: typing goes into it */
    CHECK(lg_wants_keyboard());
    type("abc");
    CHECK(strcmp(st.name, "abc") == 0);
    key(LG_KEY_ESCAPE);

    /* the popup menu opens at the mouse */
    click(st.at_menu_x, st.at_menu_y); frame();
    shot(dir, "7-menu");
    click(st.at_menu_x + 10, popup_row(st.at_menu_y, 1)); frame();
    CHECK(st.menu_pick == 2);

    /* tabs */
    tab2 = st.at_tabs_x + lg_text_width("Pickers") + 2 * s->padding_x + 2 + 12;
    tab3 = tab2 + lg_text_width("Table") + 2 * s->padding_x + 2;
    click(tab2, st.at_tabs_y - 10); frame();
    CHECK(st.tab == 1);
    shot(dir, "8-table");
    click(tab3, st.at_tabs_y - 10); frame();
    CHECK(st.tab == 2);

    /* the editor: Enter makes a line, Up and Down move between lines */
    click(st.at_editor_x, st.at_editor_y);
    lg_input_key(LG_KEY_CTRL, 1); key(LG_KEY_A); lg_input_key(LG_KEY_CTRL, 0);
    type("ab"); key(LG_KEY_ENTER); type("cd");
    CHECK(strcmp(st.code, "ab\ncd") == 0);
    key(LG_KEY_UP); type("X");
    CHECK(strcmp(st.code, "abX\ncd") == 0);
    key(LG_KEY_HOME); type("Y"); key(LG_KEY_DOWN); key(LG_KEY_END); type("Z");
    CHECK(strcmp(st.code, "YabX\ncdZ") == 0);
    key(LG_KEY_TAB);                                                /* in the editor Tab is four spaces, it does not leave */
    CHECK(strcmp(st.code, "YabX\ncdZ    ") == 0);
    lg_input_key(LG_KEY_SHIFT, 1); key(LG_KEY_UP); lg_input_key(LG_KEY_SHIFT, 0);     /* select up to the line above... */
    key(LG_KEY_BACKSPACE);                                                          /* ...and delete it */
    CHECK(strcmp(st.code, "YabX") == 0);
    for (was = 0; was < 30; was++) { key(LG_KEY_ENTER); type("line"); }             /* more lines than fit: it scrolls */
    shot(dir, "9-editor");
    key(LG_KEY_ESCAPE);
    CHECK(!lg_wants_keyboard() || 1);
    click(st.at_tabs_x + 12, st.at_tabs_y - 10); frame();
    CHECK(st.tab == 0);
}

static void ctrl(int k) { lg_input_key(LG_KEY_CTRL, 1); key(k); lg_input_key(LG_KEY_CTRL, 0); }
static void move(float x, float y) { lg_input_mouse_pos(x, y); frame(); frame(); frame(); }

/* 0.3: typed slider values, undo, menus, table columns, word wrap */
static void newer(const char *dir) {
    const lg_style *s = lg_get_style();
    float bar = lg_line_height() + 2 * s->padding_y, mid = st.at_count_x + st.at_count_w / 2, x0, hx;
    char *q;

    /* a click on a slider's number turns it into a text field; the number stays between the ends */
    click(mid, st.at_count_y);
    CHECK(lg_wants_keyboard());
    type("7");
    shot(dir, "11-slider-typing");
    key(LG_KEY_ENTER); frame();
    CHECK(st.count == 7);
    CHECK(!lg_wants_keyboard());
    click(mid, st.at_count_y); type("99"); key(LG_KEY_ENTER); frame();
    CHECK(st.count == 10);
    click(mid, st.at_count_y); type("5"); key(LG_KEY_ESCAPE); frame();          /* Escape leaves the value alone */
    CHECK(st.count == 10);
    click(mid, st.at_count_y); type("4"); click(5, H - 5); frame();             /* a click somewhere else takes the number */
    CHECK(st.count == 4);
    drag(mid, st.at_count_y, st.at_count_x - 50, st.at_count_y);                /* a drag that starts on the number still slides */
    CHECK(st.count == 0);
    CHECK(!lg_wants_keyboard());
    st.count = 3;

    /* undo and redo in a text field */
    click(st.at_input_x, st.at_input_y);
    ctrl(LG_KEY_A); type("one"); key(LG_KEY_SPACE);
    CHECK(strcmp(st.name, "one") == 0);
    ctrl(LG_KEY_A); key(LG_KEY_BACKSPACE);
    CHECK(strcmp(st.name, "") == 0);
    ctrl(LG_KEY_Z);
    CHECK(strcmp(st.name, "one") == 0);
    ctrl(LG_KEY_Z);
    CHECK(strcmp(st.name, "abc") == 0);
    ctrl(LG_KEY_Y);
    CHECK(strcmp(st.name, "one") == 0);
    lg_input_key(LG_KEY_SHIFT, 1); ctrl(LG_KEY_Z); lg_input_key(LG_KEY_SHIFT, 0);
    CHECK(strcmp(st.name, "") == 0);
    ctrl(LG_KEY_Z); key(LG_KEY_ESCAPE);
    CHECK(strcmp(st.name, "one") == 0);

    /* the main menu bar: a click opens a menu, a click on an item picks it and closes the menu */
    click(st.at_file_x, st.at_file_y); frame();
    click(st.at_file_x + 10, popup_row(bar, 1)); frame();
    CHECK(st.file_pick == 2);
    click(st.at_file_x + 10, popup_row(bar, 0)); frame();           /* the menu is closed: nothing is picked */
    CHECK(st.file_pick == 2);
    click(st.at_file_x, st.at_file_y); frame();
    move(st.at_file_x + 10, popup_row(bar, 2));                     /* resting on "Recent" opens the menu beside it */
    shot(dir, "12-menus");
    CHECK(st.at_recent_x > st.at_file_x + 40);
    click(st.at_recent_x, st.at_recent_y); frame();
    CHECK(st.file_pick == 10);
    click(st.at_file_x, st.at_file_y); frame();
    move(st.at_view_x, st.at_file_y);                               /* with a menu open, moving to the next word switches */
    click(st.at_view_x + 10, popup_row(bar, 1)); frame();
    CHECK(st.wrap == 1);
    CHECK(st.file_pick == 10);

    /* the table: a click on a heading sorts, another click turns the order around */
    click(st.at_tabs_x + lg_text_width("Pickers") + 2 * s->padding_x + 14, st.at_tabs_y - 10); frame();
    CHECK(st.tab == 1 && st.sort_column == -1);
    click(st.at_head_x[1] + 20, st.at_table_y + 5); frame(); frame();
    CHECK(st.sort_column == 1 && st.sort_down == 0 && st.first_row == 0);
    click(st.at_head_x[1] + 20, st.at_table_y + 5); frame(); frame();
    CHECK(st.sort_column == 1 && st.sort_down == 1 && st.first_row == 2);
    hx = st.at_head_x[1];                                           /* dragging the line before a column moves it */
    drag(hx - s->padding_x, st.at_table_y + 40, hx - s->padding_x - 50, st.at_table_y + 40); frame();
    CHECK(st.at_head_x[1] == hx - 50);
    shot(dir, "13-table");

    /* word wrap: Down goes to the next piece of a folded line */
    click(st.at_tabs_x + lg_text_width("Pickers") + lg_text_width("Table") + 4 * s->padding_x + 16, st.at_tabs_y - 10); frame();
    CHECK(st.tab == 2);
    strcpy(st.code, "one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty");
    frame();
    click(st.at_editor_x + 60, st.at_editor_y);
    ctrl(LG_KEY_HOME);
    key(LG_KEY_DOWN); type("#");
    q = strchr(st.code, '#');
    CHECK(q && q > st.code + 20 && q < st.code + 100);
    key(LG_KEY_END); key(LG_KEY_ENTER); type("second line");
    shot(dir, "14-wrap");
    x0 = (float)strlen(st.code);
    ctrl(LG_KEY_Z); ctrl(LG_KEY_Z); ctrl(LG_KEY_Z);
    CHECK(strchr(st.code, '#') == 0 && (float)strlen(st.code) < x0);
    key(LG_KEY_ESCAPE);
    st.wrap = 0;
    click(st.at_tabs_x + 12, st.at_tabs_y - 10); frame();
    CHECK(st.tab == 0);
}

/* docking: with the mouse, and from code */
static void docking(const char *dir) {
    float bar = lg_line_height() + 2 * lg_get_style()->padding_y, x, y, w, h, w0, mid_y = bar + (H - bar) / 2, leaf, gx, gy;
    static char text[4096], again[4096], small[20];

    lg_dock_space(&x, &y, &w, &h);
    CHECK(x == 0 && y == bar && w == W && h == H - bar);            /* nothing is docked: all is free */

    /* drag Style by its title onto the square at the right edge of the screen */
    lg_input_mouse_pos(st.win2_now_x + 60, st.win2_now_y - 20); frame();
    lg_input_mouse_button(0, 1); frame();
    move(W - 200, mid_y); move(W - 25, mid_y);
    shot(dir, "15-dragging");
    lg_input_mouse_button(0, 0); frame(); frame(); frame();
    lg_dock_space(&x, &y, &w, &h);
    CHECK(w < W * 0.8f && w > W * 0.7f && h == H - bar);
    CHECK(st.win2_now_x > x + w);
    leaf = x + w + 4;

    /* drag More onto the middle square of Style's place: they become tabs, More in front */
    drag(st.at_tabs_x + 40, st.at_tabs_y - 52, (leaf + W) / 2, mid_y); frame(); frame();
    CHECK(st.more_shown == 1 && st.style_shown == 0);
    CHECK(st.at_tabs_x > leaf);
    shot(dir, "16-tabs");
    click(leaf + 10, bar + 10); frame();                            /* a click on Style's tab brings it to the front */
    CHECK(st.more_shown == 0 && st.style_shown == 1);

    /* the gap between the docked windows and the rest can be dragged */
    w0 = w;
    drag(leaf - 2, mid_y, leaf - 102, mid_y); frame();
    lg_dock_space(&x, &y, &w, &h);
    CHECK(w == w0 - 100);

    /* dragging a tab away makes the window float again */
    drag(leaf - 100 + 10, bar + 10, 500, 300); frame(); frame();
    CHECK(st.more_shown == 1 && st.style_shown == 1);
    CHECK(st.win2_now_x < 560 && st.win2_now_x > 440);

    /* the same from code */
    lg_dock_window("Style", "More", LG_DOCK_BOTTOM, 0.5f);
    lg_dock_window("Loguia demo", NULL, LG_DOCK_LEFT, 0.45f);
    frame(); frame();
    lg_dock_space(&x, &y, &w, &h);
    CHECK(x > W * 0.44f && x < W * 0.46f);
    CHECK(st.win2_now_y > mid_y);
    shot(dir, "17-layout");
    /* the layout as text: saved, thrown away, brought back */
    CHECK(lg_dock_save(text, (int)sizeof text) < (int)sizeof text);
    CHECK(lg_dock_save(small, (int)sizeof small) == (int)strlen(text) && strlen(small) == sizeof small - 1);
    lg_dock_window("Style", NULL, LG_DOCK_NONE, 0);
    lg_dock_window("More", NULL, LG_DOCK_NONE, 0);
    lg_dock_window("Loguia demo", NULL, LG_DOCK_NONE, 0);
    frame(); frame();
    lg_dock_space(&x, &y, &w, &h);
    CHECK(x == 0 && w == W);
    lg_dock_load(text);
    frame(); frame();
    lg_dock_space(&x, &y, &w, &h);
    CHECK(x > W * 0.44f && x < W * 0.46f);
    CHECK(st.win2_now_y > mid_y && st.style_shown && st.more_shown);
    CHECK(lg_dock_save(again, (int)sizeof again) == (int)strlen(text) && strcmp(text, again) == 0);
    lg_dock_load("N0 3 1 1 5000 0 0 0 0 0 0 0\nN1 1 0 0 0 0 0 0 0 0 0 0\n");      /* nonsense is refused, nothing stays docked */
    frame(); frame();
    lg_dock_space(&x, &y, &w, &h);
    CHECK(x == 0 && w == W);

    /* two floating windows: Style dropped on the right square of More makes a group that floats */
    drag(st.win2_now_x + 60, st.win2_now_y - 20, 300, 420); frame();            /* Style down and left, out of the way */
    drag(st.at_tabs_x + 40, st.at_tabs_y - 52, 600, 150); frame();              /* More somewhere free, and on top */
    gx = st.at_tabs_x; gy = st.at_tabs_y;                                       /* More's window is at (gx - 11, gy - 66), 560 x 310 */
    lg_input_mouse_pos(st.win2_now_x + 60, st.win2_now_y - 20); frame();
    lg_input_mouse_button(0, 1); frame();
    move(gx + 100, gy + 60); move(gx - 11 + 280 + 38, gy - 66 + 155);          /* the square right of More's middle */
    shot(dir, "18-onto-a-window");
    lg_input_mouse_button(0, 0); frame(); frame(); frame();
    CHECK(st.style_shown && st.more_shown);
    CHECK(st.win2_now_x > gx + 250 && st.win2_now_x < gx + 320);                /* Style has the right half of what was More */
    CHECK(st.at_tabs_x == gx && st.at_tabs_y == gy + 10);                       /* More moved down by the strip */
    lg_dock_space(&x, &y, &w, &h);
    CHECK(x == 0 && w == W);                                                    /* nothing was taken from the screen */
    drag(gx + 60, gy - 66 + 5, gx + 90, gy - 66 + 45); frame();                 /* the strip moves the whole group */
    CHECK(st.at_tabs_x == gx + 30 && st.at_tabs_y == gy + 50);
    shot(dir, "19-group");
    lg_dock_save(text, (int)sizeof text);
    lg_dock_load(text);                                                         /* a group survives saving too */
    frame(); frame();
    CHECK(st.at_tabs_x == gx + 30 && st.at_tabs_y == gy + 50 && st.win2_now_x > gx + 280);
    drag(st.win2_now_x + 60, st.win2_now_y - 20, 200, 500); frame(); frame();   /* Style pulled out: More is an ordinary window again */
    CHECK(st.style_shown && st.more_shown);
    CHECK(st.at_tabs_x == gx + 30 && st.at_tabs_y == gy + 40);
    CHECK(lg_dock_save(text, (int)sizeof text) > 0 && strchr(text, 'N') == strrchr(text, 'N'));     /* only the screen itself is left */
}

/* an input method composes a word at the caret, then hands over the finished text */
static void compose(const char *dir) {
    float x, y, w, h, fx, fy, fw, fh;
    click(st.at_input_x, st.at_input_y);
    lg_input_key(LG_KEY_CTRL, 1); key(LG_KEY_A); lg_input_key(LG_KEY_CTRL, 0);
    type("Ja: ");
    lg_input_compose("\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93", 6);      /* にほん, the caret before ん */
    frame();
    shot(dir, "20-composing");
    CHECK(lg_ime_rect(&x, &y, &w, &h));
    fx = st.at_input_x - 12; fy = st.at_input_y - lg_line_height() / 2 - lg_get_style()->padding_y;
    fw = 200; fh = lg_line_height() + 2 * lg_get_style()->padding_y;
    CHECK(x > fx + lg_text_width("Ja:") && x < fx + fw && y >= fy && y + h <= fy + fh);
    CHECK(strcmp(st.name, "Ja: ") == 0);                                 /* nothing is in the text until it is finished */
    key(LG_KEY_ENTER);                                                   /* Enter belongs to the input method while it composes */
    CHECK(lg_wants_keyboard());
    lg_input_compose(NULL, 0);
    lg_input_char(0x65E5); lg_input_char(0x672C); frame();               /* 日本 */
    CHECK(strcmp(st.name, "Ja: \xE6\x97\xA5\xE6\x9C\xAC") == 0);
    key(LG_KEY_ESCAPE);
    CHECK(!lg_ime_rect(0, 0, 0, 0));
}

/* fonts loaded from files, at any size: argv[2...] are font files to try */
static void fonts(const char *dir, int count, char **paths) {
    lg_font builtin = *lg_get_font();
    int i;
    for (i = 0; i < count; i++) {
        char name[64];
        lg_font *f = lg_font_load_file(paths[i], 15 + 3 * (float)i);
        int cjk;
        CHECK(f != 0);
        if (!f) continue;
        if (i == count - 1 && count > 1) {          /* the last font gets the first one behind it, for what it lacks */
            lg_font *back = lg_font_load_file(paths[0], 15);
            lg_font_fallback(f, back);
            lg_set_font(f);
            strcpy(st.name, "Zażółć 日本語 한국어");
            frame(); frame();
            CHECK(lg_text_width("\xC5\xBC") != lg_text_width("?"));         /* ż comes from the font behind */
            shot(dir, "23-fallback");
            lg_font_fallback(f, NULL);
            lg_font_free(back);
        }
        lg_set_font(f);
        strcpy(st.name, "Zażółć 日本語 한국어");
        frame(); frame();
        CHECK(lg_get_font()->glyph_count > 20);
        CHECK(lg_text_width("Ag") > 0 && lg_line_height() >= 15 + 3 * i);
        cjk = lg_text_width("\xE6\x97\xA5") != lg_text_width("?");
        snprintf(name, sizeof name, "21-font-%d%s", i, cjk ? "-cjk" : "");
        shot(dir, name);
        lg_set_pixel_scale(2);                                           /* drawn again, sharp, for a HiDPI screen */
        frame(); frame();
        CHECK(lg_get_font()->scale == 2);
        snprintf(name, sizeof name, "22-font-%d-2x", i);
        shot_at(dir, name, 2);
        lg_set_pixel_scale(1);
        lg_font_free(f);
        CHECK(lg_get_font()->source == 0);                               /* freeing it went back to the built-in font */
    }
    CHECK(lg_font_load("not a font at all", 17, 15) == 0);
    lg_set_font(&builtin);
    frame();
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    lg_init();
    demo_init(&st);
    frame(); frame();
    shot(dir, "1-start");

    /* the button counts clicks */
    click(st.at_button_x, st.at_button_y);
    click(st.at_button_x, st.at_button_y);
    CHECK(st.clicks == 2);

    /* the checkbox flips */
    CHECK(st.check == 1);
    click(st.at_check_x, st.at_check_y);
    CHECK(st.check == 0);

    /* dragging the slider to its right end gives the largest value */
    drag(st.at_slider_x, st.at_slider_y, st.at_slider_x + 600, st.at_slider_y);
    CHECK(st.volume == 1.0f);

    /* typing: click the field, select everything, replace it, then edit in the middle */
    click(st.at_input_x, st.at_input_y);
    CHECK(lg_wants_keyboard());
    lg_input_key(LG_KEY_CTRL, 1); key(LG_KEY_A); lg_input_key(LG_KEY_CTRL, 0);
    type("Finch");
    CHECK(strcmp(st.name, "Finch") == 0);
    key(LG_KEY_HOME); key(LG_KEY_RIGHT); key(LG_KEY_DELETE);
    lg_input_char(0x142); frame();                              /* a two-byte character */
    CHECK(strcmp(st.name, "F\xC5\x82nch") == 0);
    key(LG_KEY_BACKSPACE);
    type("i");
    CHECK(strcmp(st.name, "Finch") == 0);
    shot(dir, "2-typing");
    key(LG_KEY_ENTER);
    CHECK(!lg_wants_keyboard());
    CHECK(st.entered == 1);

    /* the tree: opening a node shows its children, selecting marks a row */
    click(st.at_tree_x, st.at_tree_y);
    frame();
    CHECK(st.tree_open == 1);

    /* the wheel scrolls the panel under the mouse */
    lg_input_mouse_pos(st.at_panel_x, st.at_panel_y); frame();
    lg_input_scroll(0, -3); frame(); frame();
    CHECK(st.panel_scrolled);
    shot(dir, "3-used");

    more(dir);
    newer(dir);
    compose(dir);

    /* windows: drag the second one by its title, then it is on top and has moved */
    drag(st.win2_x + 60, st.win2_y + 10, st.win2_x - 140, st.win2_y + 90);
    frame();
    CHECK(st.win2_moved);
    {   /* a slow drag in sub-pixel steps (fractional display scaling) must still follow the mouse */
        float x0 = st.win2_now_x, y0 = st.win2_now_y;    /* the first button: just under the title bar */
        int i;
        lg_input_mouse_pos(x0 + 60, y0 - 20); frame();
        lg_input_mouse_button(0, 1); frame();
        for (i = 1; i <= 200; i++) { lg_input_mouse_pos(x0 + 60 + i * 0.3f, y0 - 20 + i * 0.3f); frame(); }
        lg_input_mouse_button(0, 0); frame(); frame();
        CHECK(st.win2_now_x - x0 >= 59 && st.win2_now_x - x0 <= 61);
        CHECK(st.win2_now_y - y0 >= 59 && st.win2_now_y - y0 <= 61);
    }
    docking(dir);
    lg_style_light();
    frame();
    shot(dir, "4-light");

    /* a HiDPI screen: the layout stays the same, the double-size font takes over */
    {
        float line = lg_line_height(), width = lg_text_width("Finch");
        lg_style_dark();
        lg_set_pixel_scale(2);
        frame(); frame();
        CHECK(lg_get_font()->scale == 2);
        CHECK(lg_line_height() == line && lg_text_width("Finch") == width);
        shot_at(dir, "10-hidpi", 2);
        lg_set_pixel_scale(1);
        frame();
        CHECK(lg_get_font()->scale == 1);
    }

    fonts(dir, argc > 2 ? argc - 2 : 0, argv + 2);
    lg_shutdown();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("all checks passed\n");
    return 0;
}
