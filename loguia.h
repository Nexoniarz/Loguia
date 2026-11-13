/* Loguia - a tiny immediate-mode GUI in one C file.

   Copyright 2026 Nexoniarz
   SPDX-License-Identifier: Apache-2.0
   Licensed under the Apache License, Version 2.0. You may obtain a copy of the License at
   http://www.apache.org/licenses/LICENSE-2.0 (it is also in the LICENSE file next to this one).
   The software is distributed "AS IS", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND.
   (The built-in font is JetBrains Mono, under the SIL Open Font License 1.1.)

   USING IT
       In every file that calls Loguia:        #include "loguia.h"
       In exactly ONE .c file, before that:    #define LOGUIA_IMPLEMENTATION

   The core knows nothing about your system. You tell it what the mouse and keyboard did,
   you describe the interface every frame, and it gives back a list of triangles to draw:

       lg_init();
       for each frame:
           lg_input_mouse_pos(x, y);  lg_input_mouse_button(0, down);  ...
           lg_begin_frame(width, height, seconds_since_last_frame);
           if (lg_begin("Hello", 20, 20, 300, 200, 0)) {
               lg_text("Some text");
               if (lg_button("Press me")) { ... }
           }
           lg_end();
           lg_end_frame();
           draw lg_get_draw_list() with whatever you like

   OPTIONAL MODULES (define next to LOGUIA_IMPLEMENTATION)
       LOGUIA_GL        lg_gl_*     draws with OpenGL 1.1 (any compatibility / legacy context)
       LOGUIA_GLFW      lg_glfw_*   input and timing from a GLFW window
                                    (Linux and BSD on X11 or Wayland, macOS, Windows)
       LOGUIA_SOFTWARE  lg_soft_*   draws into a plain RGBA pixel buffer, no GPU needed
       LOGUIA_WINDOW    lg_window_* a native window with no library at all: Win32 on Windows,
                                    Cocoa on macOS, X11 on Linux and BSD. Draws with LOGUIA_SOFTWARE.
       LOGUIA_WAYLAND   with LOGUIA_WINDOW on Linux and BSD: talk to Wayland directly, and use
                                    X11 only where that is not possible (LOGUIA_NO_X11 leaves X11 out)

   FONTS AND LANGUAGES
       The built-in font covers Latin scripts. lg_font_load() reads any TrueType or OpenType font
       at any size, Chinese, Japanese and Korean ones too, and lg_font_fallback() chains them.
       Input methods work in every text field (lg_input_compose, lg_ime_rect; the window module
       connects them to the system).

   COMPILE-TIME SETTINGS (define before the implementation)
       LOGUIA_STATIC                 make every function static (private to one file)
       LOGUIA_MALLOC / LOGUIA_REALLOC / LOGUIA_FREE     your own allocator
       LOGUIA_ASSERT(x)              your own assert
       LOGUIA_NO_DEFAULT_FONT        leave the built-in font out; call lg_set_font yourself
       LOGUIA_NO_FONT_2X             leave out the double-size copy of the font (used on HiDPI screens)
       LOGUIA_NO_STDIO               no <stdio.h>: drops lg_textf
       LOGUIA_MAX_BOXES              windows + panels remembered at once (default 64)
       LOGUIA_STATE_SLOTS            remembered open/closed tree nodes (default 1024, power of two)
       LOGUIA_ID_STACK, LOGUIA_CLIP_STACK                nesting depth (default 32)
       LOGUIA_MAX_POPUPS             popups open inside each other (default 4)
       LOGUIA_MAX_COLUMNS            columns in a table (default 16)
       LOGUIA_MAX_DOCKS              pieces the screen can be split into by docked windows (default 32)
       LOGUIA_UNDO_LEVELS            steps Ctrl+Z can take back in a text field (default 64)
       LOGUIA_GL_HEADER              the OpenGL header to include, e.g. <glad/gl.h>

   RUN-TIME SETTINGS
       lg_get_style() returns every colour and size; change any of them at any time.
       lg_font_load() reads a TrueType or OpenType font at any size; lg_set_font() switches to it.
       lg_set_clipboard() connects copy and paste.

   KEYBOARD
       Tab and Shift+Tab (or Up and Down) walk through the widgets, Enter or Space presses the
       one that is marked, Left and Right move sliders and open or close tree nodes, Escape
       closes a popup. lg_set_keyboard_nav(0) turns this off.
       In text: arrows, Home, End, PageUp, PageDown move; Shift selects; Ctrl+A, C, X, V;
       Ctrl+Z takes a change back, Ctrl+Y or Ctrl+Shift+Z brings it again.

   Colours are written 0xRRGGBBAA, for example 0xFF8800FF is opaque orange.

   Labels are also names: two widgets in one window need different labels. Text after "##"
   is part of the name but is not shown: "Delete##row1", "Delete##row2", or just "##field".
*/
#if defined(LOGUIA_IMPLEMENTATION) && defined(LOGUIA_WINDOW) && !defined(_WIN32) && !defined(__APPLE__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE     /* LOGUIA_WINDOW reads the clock: with -std=c99 that needs this, before any system header */
#endif
#ifndef LOGUIA_H
#define LOGUIA_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LGDEF
#if defined(LOGUIA_STATIC) && defined(__GNUC__)
#define LGDEF static __attribute__((unused))
#elif defined(LOGUIA_STATIC)
#define LGDEF static
#else
#define LGDEF extern
#endif
#endif

#define LOGUIA_VERSION "0.4.0"

/* ---------------------------------------------------------------- style */

enum {
    LG_COL_TEXT, LG_COL_TEXT_DIM,
    LG_COL_WINDOW, LG_COL_TITLE, LG_COL_TITLE_ACTIVE, LG_COL_BORDER, LG_COL_PANEL,
    LG_COL_BUTTON, LG_COL_BUTTON_HOVER, LG_COL_BUTTON_ACTIVE,
    LG_COL_FRAME, LG_COL_FRAME_HOVER,          /* text fields, sliders, checkboxes */
    LG_COL_ACCENT,                             /* check marks, slider handles, progress, focus */
    LG_COL_SELECTED, LG_COL_HOVER,             /* rows in trees and lists */
    LG_COL_SEPARATOR,
    LG_COL_SCROLLBAR, LG_COL_SCROLL_THUMB, LG_COL_SCROLL_THUMB_HOVER,
    LG_COL_TEXT_SELECT,
    LG_COL_COUNT
};

typedef struct lg_style {
    unsigned int colors[LG_COL_COUNT];  /* 0xRRGGBBAA */
    float padding_x, padding_y;         /* space inside a widget, around its text */
    float spacing_x, spacing_y;         /* space between widgets */
    float window_padding;               /* space between a window's edge and its contents */
    float indent;                       /* how far tree children move right */
    float scrollbar_size;
    float border;                       /* outline thickness, 0 for none */
    float rounding;                     /* corner radius, 0 for square corners */
    float font_scale;                   /* 1 = the size the font was made for */
} lg_style;

LGDEF lg_style *lg_get_style(void);
LGDEF void lg_style_dark(void);
LGDEF void lg_style_light(void);
LGDEF void lg_set_color(int which, unsigned int rgba);
LGDEF unsigned int lg_rgba(int r, int g, int b, int a);

/* ---------------------------------------------------------------- font */

/* One character in the font picture. x,y,w,h: where it is in the picture. xo,yo: where to
   put it relative to the pen (top of the line). adv: how far the pen moves afterwards. */
typedef struct lg_glyph {
    unsigned int cp;
    unsigned short x, y, w, h;
    short xo, yo;
    unsigned short adv;
} lg_glyph;

/* pixels: one byte per pixel (0 = empty, 255 = solid). The 2x2 pixels at the top-left corner
   must be 255: Loguia uses them to draw solid rectangles. Glyphs must be sorted by cp.
   Loguia keeps the pointers; the data has to stay alive. */
typedef struct lg_font {
    const unsigned char *pixels;
    int width, height;
    const lg_glyph *glyphs;
    int glyph_count;
    float line_height;
    float scale;        /* how many times bigger than normal the picture is: 2 for a font made at
                           double size for HiDPI screens. 0 means 1. */
    unsigned int version;   /* goes up whenever the picture changes: a renderer of your own uploads it again then */
    void *source;           /* a font from lg_font_load: where its characters come from. NULL otherwise */
} lg_font;

/* A font from a TrueType or OpenType file (.ttf, .otf, or the first font in a .ttc), at a size of
   `pixels`: any size, and it can be loaded again at another one while the program runs. Each
   character is drawn into the font picture the first time it is shown, so a font with tens of
   thousands of them (Chinese, Japanese, Korean) costs only what is on screen. HiDPI screens get
   it drawn sharp at their own scale. The data has to stay alive as long as the font.
   Returns NULL when the data is not a font. Use it with lg_set_font; lg_font_free when done. */
LGDEF lg_font *lg_font_load(const void *data, int size, float pixels);
#ifndef LOGUIA_NO_STDIO
LGDEF lg_font *lg_font_load_file(const char *path, float pixels);
#endif
LGDEF void lg_font_free(lg_font *font);
/* Characters that font does not have are taken from other (also from lg_font_load), at font's
   size and on its line: a Chinese font with a Latin one behind it, for example. Several can be
   chained. other has to stay alive as long as font is used; NULL takes the fallback away. */
LGDEF void lg_font_fallback(lg_font *font, lg_font *other);

LGDEF void lg_set_font(const lg_font *font);
LGDEF const lg_font *lg_get_font(void);
LGDEF float lg_text_width(const char *text);
LGDEF float lg_line_height(void);
/* How many screen pixels one unit of the interface covers (1.5 on a screen scaled to 150 %).
   Loguia then keeps text and curves sharp: from 1.25 up it uses the double-size built-in font.
   The GLFW and window modules call this for you. */
LGDEF void lg_set_pixel_scale(float scale);

/* ---------------------------------------------------------------- setup and frames */

LGDEF void lg_init(void);
LGDEF void lg_shutdown(void);
LGDEF void lg_begin_frame(float width, float height, float dt);
LGDEF void lg_end_frame(void);

/* ---------------------------------------------------------------- input */

enum {
    LG_KEY_NONE, LG_KEY_LEFT, LG_KEY_RIGHT, LG_KEY_UP, LG_KEY_DOWN, LG_KEY_HOME, LG_KEY_END,
    LG_KEY_BACKSPACE, LG_KEY_DELETE, LG_KEY_ENTER, LG_KEY_TAB, LG_KEY_ESCAPE,
    LG_KEY_CTRL, LG_KEY_SHIFT, LG_KEY_A, LG_KEY_C, LG_KEY_V, LG_KEY_X,
    LG_KEY_SPACE, LG_KEY_PAGE_UP, LG_KEY_PAGE_DOWN, LG_KEY_Z, LG_KEY_Y,
    LG_KEY_COUNT
};

LGDEF void lg_input_mouse_pos(float x, float y);
LGDEF void lg_input_mouse_button(int button, int down);     /* 0 left, 1 right, 2 middle */
LGDEF void lg_input_scroll(float dx, float dy);
LGDEF void lg_input_key(int key, int down);                 /* call again for key repeats */
LGDEF void lg_input_char(unsigned int codepoint);           /* typed text, as Unicode */
LGDEF void lg_set_clipboard(const char *(*get)(void *user), void (*set)(void *user, const char *text), void *user);
LGDEF int lg_wants_mouse(void);      /* the mouse is over the interface: don't use the click yourself */
LGDEF int lg_wants_keyboard(void);   /* a text field is being typed into, or a widget is marked */
/* Input methods (Chinese, Japanese, Korean and others). While the user composes a word, its
   text is shown at the caret: pass it here, with the caret as a byte offset into it, and NULL
   or "" when the composing ends. The finished text arrives through lg_input_char as usual.
   lg_ime_rect says where the caret of the field being typed into is, so the system can put its
   list of candidates beside it: 0 when no field is being typed into. The window modules do both. */
LGDEF void lg_input_compose(const char *text, int caret);
LGDEF int lg_ime_rect(float *x, float *y, float *w, float *h);
LGDEF void lg_set_keyboard_nav(int on);     /* Tab / arrows / Enter between widgets; on by default */

/* ---------------------------------------------------------------- windows and panels */

enum {
    LG_WINDOW_NO_TITLE      = 1,
    LG_WINDOW_NO_MOVE       = 2,
    LG_WINDOW_NO_RESIZE     = 4,
    LG_WINDOW_NO_SCROLL     = 8,
    LG_WINDOW_NO_BACKGROUND = 16,
    LG_WINDOW_FILL          = 32,    /* always covers the whole screen, behind every other window */
    LG_WINDOW_DOCK          = 64     /* the user may dock it: see "docking" below */
};

/* x, y, w, h are only where the window starts; the user can move and resize it after that.
   lg_begin returns 0 only for a docked window that is behind another tab: nothing of it is
   shown then, so its contents may be skipped. Always call lg_end(), whatever it returned. */
LGDEF int lg_begin(const char *title, float x, float y, float w, float h, int flags);
LGDEF void lg_end(void);

/* Docking. A window made with LG_WINDOW_DOCK can be dragged by its title onto one of the
   squares that appear then: at an edge of the screen, or beside, above, below or onto (as a
   tab) a window that is already docked. Dragging its title or its tab away makes it float
   again. Windows without the flag are left alone by the mouse. Dropped onto a floating window that
   has the flag too, the two become a group that floats: the strip along its top moves it,
   its corner resizes it.
   lg_dock_window does the same from code, once, for the layout a program starts with:
       lg_dock_window("Files", NULL, LG_DOCK_LEFT, 0.2f);         a fifth of the screen, on the left
       lg_dock_window("Output", NULL, LG_DOCK_BOTTOM, 0.3f);
       lg_dock_window("Problems", "Output", LG_DOCK_TAB, 0);      a tab next to Output
   next_to: the title of another window, or NULL for the whole screen. share: the part of
   that space the window takes, 0 for the usual. LG_DOCK_NONE makes the window float.
   With NULL, LG_DOCK_TAB fills the middle that the docked windows leave free. */
enum { LG_DOCK_NONE, LG_DOCK_LEFT, LG_DOCK_RIGHT, LG_DOCK_TOP, LG_DOCK_BOTTOM, LG_DOCK_TAB };
LGDEF void lg_dock_window(const char *title, const char *next_to, int side, float share);
/* the part of the screen that docked windows and the main menu bar leave free, for your own drawing */
LGDEF void lg_dock_space(float *x, float *y, float *w, float *h);
/* Keeping the layout between runs. lg_dock_save writes what is docked where, and where the
   floating windows are, as text into buffer, and returns how long that text is: when that is
   not below capacity, it did not fit. lg_dock_load puts such a text back (after lg_init, before
   or between frames). Windows are known by their titles; keeping the text in a file is up to you. */
LGDEF int lg_dock_save(char *buffer, int capacity);
LGDEF void lg_dock_load(const char *text);

/* A scrolling area inside a window. w, h: above 0 is a size in pixels, 0 means "all the
   space that is left", below 0 means "all that is left except this much". */
LGDEF void lg_begin_panel(const char *name, float w, float h, int flags);
LGDEF void lg_end_panel(void);

/* A popup floats above everything and closes on a click outside it or on Escape.
       if (lg_button("Menu")) lg_open_popup("menu");
       if (lg_begin_popup("menu")) { if (lg_selectable("Save", 0)) save(); lg_end_popup(); }
   A selectable row closes the popup it is in. Call lg_end_popup() only when begin returned 1. */
LGDEF void lg_open_popup(const char *name);         /* appears at the mouse */
LGDEF int lg_begin_popup(const char *name);
LGDEF void lg_end_popup(void);
LGDEF void lg_close_popup(void);                    /* closes the popup being filled */
LGDEF void lg_tooltip(const char *text);            /* shown while the mouse rests on the last widget */

/* Menus. A menu bar belongs to the window it is called in, right after lg_begin. Called
   outside every window it is the main menu bar along the top of the screen.
       if (lg_begin_menu_bar()) {
           if (lg_begin_menu("File")) {
               if (lg_menu_item("Open...", "Ctrl+O", 0)) open();
               if (lg_begin_menu("Recent")) { ...; lg_end_menu(); }       a menu inside a menu
               lg_end_menu();
           }
           lg_end_menu_bar();
       }
   Call an end function only when its begin returned 1. lg_begin_menu and lg_menu_item also
   work inside any popup. shortcut is only shown (it may be NULL); checked draws a check mark. */
LGDEF int lg_begin_menu_bar(void);
LGDEF void lg_end_menu_bar(void);
LGDEF int lg_begin_menu(const char *label);
LGDEF void lg_end_menu(void);
LGDEF int lg_menu_item(const char *label, const char *shortcut, int checked);       /* 1 when picked */

/* A table: lg_table_next() moves to the next cell (call it before every cell, the first too)
   and starts a new row by itself. widths has one number per column: a width in pixels, or 0
   to share what is left; NULL makes all columns equal. */
enum {
    LG_TABLE_BORDERS = 1,
    LG_TABLE_STRIPES = 2,
    LG_TABLE_HEADER  = 4,     /* the first row is a heading */
    LG_TABLE_RESIZE  = 8,     /* the user can drag the lines between the columns */
    LG_TABLE_SORT    = 16     /* a click on a heading picks the column to sort by (with LG_TABLE_HEADER) */
};
LGDEF void lg_begin_table(const char *name, int columns, const float *widths, int flags);
LGDEF void lg_table_next(void);
LGDEF void lg_end_table(void);
/* Inside a table with LG_TABLE_SORT: the column the user picked and which way. Returns 0 while
   none is picked. Loguia only remembers the choice: put your rows in that order yourself. */
LGDEF int lg_table_sort(int *column, int *descending);

/* ---------------------------------------------------------------- layout */

LGDEF void lg_same_line(void);                 /* put the next widget right of the last one */
LGDEF void lg_set_next_width(float w);         /* same meaning as a panel's w */
LGDEF void lg_spacing(float height);
LGDEF void lg_indent(float w);                 /* 0 = the style's indent */
LGDEF void lg_unindent(float w);
LGDEF void lg_separator(void);
LGDEF float lg_available_width(void);

LGDEF void lg_push_id(int n);                  /* use in loops, so repeated labels stay distinct */
LGDEF void lg_push_id_str(const char *s);
LGDEF void lg_pop_id(void);

/* ---------------------------------------------------------------- widgets */

enum { LG_TREE_LEAF = 1, LG_TREE_SELECTED = 2, LG_TREE_DEFAULT_OPEN = 4, LG_TREE_OPEN_ON_ARROW = 8 };
enum { LG_INPUT_CHANGED = 1, LG_INPUT_ENTER = 2 };
enum {
    LG_EDIT_WRAP            = 1,     /* long lines are folded to the width of the editor */
    LG_EDIT_READ_ONLY       = 2,     /* the text can be selected and copied, not changed */
    LG_EDIT_NO_LINE_NUMBERS = 4
};

LGDEF void lg_text(const char *text);
LGDEF void lg_text_colored(unsigned int rgba, const char *text);
LGDEF void lg_text_wrapped(const char *text);
#ifndef LOGUIA_NO_STDIO
LGDEF void lg_textf(const char *fmt, ...);
#endif
LGDEF int lg_button(const char *label);                              /* 1 when clicked */
LGDEF int lg_checkbox(const char *label, int *value);                /* 1 when changed */
/* Sliders: 1 when the value changed. A click on the number (or Enter) lets the user type
   one instead; what was typed is kept between lo and hi. */
LGDEF int lg_slider(const char *label, float *value, float lo, float hi);
LGDEF int lg_slider_int(const char *label, int *value, int lo, int hi);
LGDEF int lg_input_text(const char *label, char *buffer, int capacity);   /* LG_INPUT_* bits */
/* An editor for text with many lines, with line numbers and undo. w, h as for a panel.
   The label is only its name. flags: LG_EDIT_* (0 for none). Returns LG_INPUT_CHANGED or 0. */
LGDEF int lg_input_text_multiline(const char *label, char *buffer, int capacity, float w, float h, int flags);
/* A drop-down list: 1 when another item was picked. */
LGDEF int lg_combo(const char *label, int *current, const char *const *items, int count);
/* A drop-down with your own contents: when it returns 1, fill it and call lg_end_popup(). */
LGDEF int lg_begin_combo(const char *label, const char *preview);
/* A row of tabs: 1 when another tab was picked. Show what belongs to labels[*current] below it. */
LGDEF int lg_tabs(const char *name, const char *const *labels, int count, int *current);
LGDEF int lg_selectable(const char *label, int selected);            /* 1 when clicked */
LGDEF void lg_progress(float fraction, const char *text);            /* text may be NULL */
LGDEF int lg_header(const char *label, int default_open);            /* 1 while open */
LGDEF void lg_image(void *texture, float w, float h);

/* Returns 1 when the node is open: then show its children and call lg_tree_end().
   A leaf always returns 0. */
LGDEF int lg_tree_begin(const char *label, int flags);
LGDEF void lg_tree_end(void);

/* About the widget that was just made. */
LGDEF int lg_item_hovered(void);
LGDEF int lg_item_clicked(void);     /* the mouse button went down on it this frame */
LGDEF int lg_item_active(void);      /* it is being held or dragged */
LGDEF void lg_item_rect(float *x, float *y, float *w, float *h);     /* where it is on screen */

/* ---------------------------------------------------------------- making your own widgets */

LGDEF void lg_reserve(float w, float h, float *x, float *y);         /* take space in the layout */
LGDEF int lg_interact(const char *name, float x, float y, float w, float h);   /* 1 when clicked */
LGDEF void lg_draw_rect(float x, float y, float w, float h, unsigned int rgba);
LGDEF void lg_draw_rounded(float x, float y, float w, float h, float radius, unsigned int rgba);
LGDEF void lg_draw_outline(float x, float y, float w, float h, float thickness, unsigned int rgba);
LGDEF void lg_draw_line(float x0, float y0, float x1, float y1, float thickness, unsigned int rgba);
LGDEF void lg_draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, unsigned int rgba);
LGDEF void lg_draw_text(float x, float y, const char *text, unsigned int rgba);
LGDEF void lg_draw_image(void *texture, float x, float y, float w, float h, unsigned int rgba);
LGDEF void lg_mouse(float *x, float *y);
LGDEF int lg_mouse_pressed(int button);        /* the button went down this frame */

/* ---------------------------------------------------------------- output */

typedef struct lg_vertex { float x, y, u, v; unsigned char r, g, b, a; } lg_vertex;

/* Draw index_count indices starting at first_index, only inside the clip rectangle.
   texture is NULL for the font picture, otherwise what you gave to lg_image. */
typedef struct lg_cmd {
    float clip_x, clip_y, clip_w, clip_h;
    void *texture;
    unsigned int first_index, index_count;
} lg_cmd;

typedef struct lg_draw_list {
    const lg_vertex *vertices;  int vertex_count;
    const unsigned int *indices; int index_count;     /* triangles */
    const lg_cmd *cmds;         int cmd_count;        /* in drawing order */
} lg_draw_list;

LGDEF const lg_draw_list *lg_get_draw_list(void);     /* valid after lg_end_frame */

/* ---------------------------------------------------------------- optional modules */

/* With LOGUIA_STATIC only the modules that are switched on are declared. */
#if !defined(LOGUIA_STATIC) || defined(LOGUIA_GL)
/* LOGUIA_GL */
LGDEF void lg_gl_init(void);                                    /* needs a current GL context */
LGDEF void lg_gl_render(int framebuffer_w, int framebuffer_h);
LGDEF void lg_gl_shutdown(void);
#endif

#if !defined(LOGUIA_STATIC) || defined(LOGUIA_GLFW)
/* LOGUIA_GLFW */
struct GLFWwindow;
LGDEF void lg_glfw_init(struct GLFWwindow *window);            /* installs GLFW callbacks */
LGDEF void lg_glfw_begin_frame(void);                          /* instead of lg_begin_frame */
#endif

/* LOGUIA_SOFTWARE: blends the interface over a w*h picture, 4 bytes (R,G,B,A) per pixel.
   The picture may be bigger than the interface (HiDPI): the interface is stretched to fill it.
   Other textures than the font are drawn as plain colour. */
#if !defined(LOGUIA_STATIC) || defined(LOGUIA_SOFTWARE) || defined(LOGUIA_WINDOW)
LGDEF void lg_soft_render(unsigned char *rgba, int w, int h);
#endif

/* LOGUIA_WINDOW: a window, its input, the clipboard and the drawing, with nothing but the
   system's own library.
       if (!lg_window_open("Title", 900, 600)) return 1;      (calls lg_init itself)
       while (lg_window_begin_frame()) {
           ... the interface ...
           lg_window_end_frame(0x0E1116FF);                   (background colour)
       }
       lg_window_close();
   lg_window_begin_frame sleeps while nothing happens, so an idle program uses no CPU.
   Linking: -lX11 on Linux and BSD, -lgdi32 on Windows, -framework Cocoa on macOS.
   With LOGUIA_WAYLAND also -lwayland-client -lxkbcommon: the window is then a real Wayland
   window (sharp on screens scaled to 125 % or 150 %), and an X11 one only when there is no
   Wayland or the compositor does not draw title bars for its programs (GNOME). */
#if !defined(LOGUIA_STATIC) || defined(LOGUIA_WINDOW)
LGDEF int lg_window_open(const char *title, int width, int height);
LGDEF int lg_window_begin_frame(void);                         /* 0 once the window was closed */
LGDEF void lg_window_end_frame(unsigned int background_rgba);
LGDEF void lg_window_close(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* LOGUIA_H */

/* ======================================================================================
   IMPLEMENTATION
   ====================================================================================== */
#if defined(LOGUIA_IMPLEMENTATION) && !defined(LOGUIA_IMPLEMENTATION_DONE)
#define LOGUIA_IMPLEMENTATION_DONE

#include <string.h>
#ifndef LOGUIA_NO_STDIO
#include <stdio.h>
#include <stdarg.h>
#endif
#ifndef LOGUIA_MALLOC
#include <stdlib.h>
#define LOGUIA_MALLOC(n) malloc(n)
#define LOGUIA_REALLOC(p, n) realloc(p, n)
#define LOGUIA_FREE(p) free(p)
#endif
#ifndef LOGUIA_ASSERT
#include <assert.h>
#define LOGUIA_ASSERT(x) assert(x)
#endif
#ifndef LOGUIA_MAX_BOXES
#define LOGUIA_MAX_BOXES 64
#endif
#ifndef LOGUIA_STATE_SLOTS
#define LOGUIA_STATE_SLOTS 1024
#endif
#ifndef LOGUIA_ID_STACK
#define LOGUIA_ID_STACK 32
#endif
#ifndef LOGUIA_CLIP_STACK
#define LOGUIA_CLIP_STACK 32
#endif
#ifndef LOGUIA_MAX_POPUPS
#define LOGUIA_MAX_POPUPS 4
#endif
#ifndef LOGUIA_MAX_COLUMNS
#define LOGUIA_MAX_COLUMNS 16
#endif
#ifndef LOGUIA_MAX_DOCKS
#define LOGUIA_MAX_DOCKS 32
#endif
#ifndef LOGUIA_UNDO_LEVELS
#define LOGUIA_UNDO_LEVELS 64
#endif
#if defined(LOGUIA_WINDOW) && !defined(LOGUIA_SOFTWARE)
#define LOGUIA_SOFTWARE
#endif

#ifndef LOGUIA_NO_DEFAULT_FONT
/* LG_FONT_BEGIN: made by tools/bake_font.c, do not edit by hand.
   Font: JetBrains Mono (SIL Open Font License 1.1), 15 px. */
#define LG__ATLAS_W 256
#define LG__ATLAS_H 152
#define LG__FONT_LINE 20
#define LG__FONT_GLYPHS 332
static const lg_glyph lg__glyphs[LG__FONT_GLYPHS] = {
{32,0,0,0,0,0,15,9},{33,189,81,3,12,3,4,9},{34,111,144,5,5,2,4,9},{35,34,108,9,11,0,4,9},{36,3,0,7,17,1,1,9},{37,211,51,9,13,0,3,9},
{38,221,51,10,13,0,3,9},{39,117,144,3,5,3,4,9},{40,8,18,6,15,2,2,9},{41,15,18,6,15,1,2,9},{42,110,133,9,9,0,5,9},{43,0,144,9,8,0,6,9},
{44,49,144,3,6,3,12,9},{45,200,144,5,2,2,9,9},{46,148,144,3,4,3,12,9},{47,22,18,7,15,1,2,9},{48,232,51,7,13,1,3,9},{49,44,108,8,11,1,4,9},
{50,193,81,7,12,1,3,9},{51,201,81,7,12,1,4,9},{52,53,108,7,11,1,4,9},{53,209,81,7,12,1,4,9},{54,217,81,9,12,0,4,9},{55,61,108,8,11,1,4,9},
{56,240,51,9,13,0,3,9},{57,227,81,9,12,0,3,9},{58,240,121,3,10,3,6,9},{59,237,81,3,12,3,6,9},{60,120,133,7,9,1,6,9},{61,53,144,7,6,1,7,9},
{62,128,133,7,9,1,6,9},{63,241,81,7,12,1,4,9},{64,30,18,9,15,0,3,9},{65,70,108,9,11,0,4,9},{66,80,108,7,11,1,4,9},{67,0,67,7,13,1,3,9},
{68,88,108,7,11,1,4,9},{69,96,108,7,11,1,4,9},{70,104,108,7,11,1,4,9},{71,8,67,7,13,1,3,9},{72,112,108,7,11,1,4,9},{73,120,108,7,11,1,4,9},
{74,0,95,8,12,0,4,9},{75,128,108,8,11,1,4,9},{76,137,108,8,11,1,4,9},{77,146,108,7,11,1,4,9},{78,154,108,7,11,1,4,9},{79,16,67,7,13,1,3,9},
{80,162,108,8,11,1,4,9},{81,40,18,7,15,1,3,9},{82,171,108,8,11,1,4,9},{83,24,67,7,13,1,3,9},{84,180,108,9,11,0,4,9},{85,9,95,7,12,1,4,9},
{86,190,108,9,11,0,4,9},{87,200,108,9,11,0,4,9},{88,210,108,9,11,0,4,9},{89,220,108,9,11,0,4,9},{90,230,108,7,11,1,4,9},{91,48,18,4,15,3,2,9},
{92,53,18,7,15,1,2,9},{93,61,18,4,15,2,2,9},{94,61,144,7,6,1,4,9},{95,206,144,9,2,0,15,9},{96,182,144,4,3,2,3,9},{97,244,121,7,10,1,6,9},
{98,17,95,7,12,1,4,9},{99,0,133,7,10,1,6,9},{100,25,95,7,12,1,4,9},{101,8,133,7,10,1,6,9},{102,238,108,8,11,0,4,9},{103,33,95,7,12,1,6,9},
{104,247,108,7,11,1,4,9},{105,41,95,8,12,1,3,9},{106,66,18,6,15,1,3,9},{107,0,121,8,11,1,4,9},{108,9,121,9,11,0,4,9},{109,136,133,9,9,0,6,9},
{110,146,133,7,9,1,6,9},{111,16,133,7,10,1,6,9},{112,50,95,7,12,1,6,9},{113,58,95,7,12,1,6,9},{114,154,133,7,9,1,6,9},{115,24,133,7,10,1,6,9},
{116,19,121,8,11,0,4,9},{117,32,133,7,10,1,6,9},{118,162,133,9,9,0,6,9},{119,172,133,9,9,0,6,9},{120,182,133,9,9,0,6,9},{121,66,95,9,12,0,6,9},
{122,192,133,7,9,1,6,9},{123,73,18,7,15,1,2,9},{124,81,18,3,15,3,2,9},{125,85,18,7,15,1,2,9},{126,152,144,7,4,1,8,9},{160,0,0,0,0,0,15,9},
{161,76,95,3,12,3,6,9},{162,136,51,7,14,1,4,9},{163,80,95,8,12,1,3,9},{164,200,133,9,9,0,5,9},{165,28,121,9,11,0,4,9},{166,93,18,3,15,3,2,9},
{167,97,18,7,15,1,3,9},{168,187,144,7,3,1,3,9},{169,38,121,9,11,0,3,9},{170,69,144,5,6,2,3,9},{171,10,144,9,8,0,7,9},{172,160,144,7,4,1,9,9},
{173,216,144,5,2,2,9,9},{174,48,121,9,11,0,3,9},{175,222,144,5,2,2,3,9},{176,75,144,5,6,2,3,9},{177,210,133,9,9,0,6,9},{178,20,144,5,8,2,2,9},
{179,26,144,5,8,2,2,9},{180,195,144,4,3,3,3,9},{181,89,95,7,12,1,6,9},{182,144,51,8,14,0,4,9},{183,168,144,3,4,3,8,9},{184,121,144,4,5,3,14,9},
{185,32,144,6,8,2,2,9},{186,81,144,5,6,2,3,9},{187,39,144,9,8,0,7,9},{188,58,121,9,11,0,4,9},{189,68,121,9,11,0,4,9},{190,78,121,9,11,0,4,9},
{191,97,95,7,12,1,6,9},{192,105,18,9,15,0,0,9},{193,115,18,9,15,0,0,9},{194,125,18,9,15,0,0,9},{195,135,18,9,15,0,0,9},{196,145,18,9,15,0,0,9},
{197,155,18,9,15,0,0,9},{198,88,121,9,11,0,4,9},{199,19,0,7,16,1,3,9},{200,165,18,7,15,1,0,9},{201,173,18,7,15,1,0,9},{202,181,18,7,15,1,0,9},
{203,189,18,7,15,1,0,9},{204,197,18,7,15,1,0,9},{205,205,18,7,15,1,0,9},{206,213,18,7,15,1,0,9},{207,221,18,7,15,1,0,9},{208,98,121,8,11,0,4,9},
{209,229,18,7,15,1,0,9},{210,27,0,7,16,1,0,9},{211,35,0,7,16,1,0,9},{212,43,0,7,16,1,0,9},{213,51,0,7,16,1,0,9},{214,59,0,7,16,1,0,9},
{215,87,144,7,6,1,7,9},{216,32,67,9,13,0,3,9},{217,67,0,7,16,1,0,9},{218,75,0,7,16,1,0,9},{219,83,0,7,16,1,0,9},{220,91,0,7,16,1,0,9},
{221,237,18,9,15,0,0,9},{222,107,121,8,11,1,4,9},{223,105,95,8,12,1,3,9},{224,42,67,7,13,1,3,9},{225,50,67,7,13,1,3,9},{226,58,67,7,13,1,3,9},
{227,66,67,7,13,1,3,9},{228,74,67,7,13,1,3,9},{229,153,51,7,14,1,2,9},{230,40,133,9,10,0,6,9},{231,82,67,7,13,1,6,9},{232,90,67,7,13,1,3,9},
{233,98,67,7,13,1,3,9},{234,106,67,7,13,1,3,9},{235,114,67,7,13,1,3,9},{236,114,95,8,12,1,3,9},{237,123,95,8,12,1,3,9},{238,132,95,8,12,1,3,9},
{239,141,95,8,12,1,3,9},{240,122,67,7,13,1,3,9},{241,150,95,7,12,1,3,9},{242,130,67,7,13,1,3,9},{243,138,67,7,13,1,3,9},{244,146,67,7,13,1,3,9},
{245,154,67,7,13,1,3,9},{246,162,67,7,13,1,3,9},{247,50,133,7,10,1,5,9},{248,58,133,9,10,0,6,9},{249,170,67,7,13,1,3,9},{250,178,67,7,13,1,3,9},
{251,186,67,7,13,1,3,9},{252,194,67,7,13,1,3,9},{253,0,35,9,15,0,3,9},{254,161,51,7,14,1,4,9},{255,10,35,9,15,0,3,9},{256,169,51,9,14,0,1,9},
{257,202,67,7,13,1,3,9},{258,20,35,9,15,0,0,9},{259,210,67,7,13,1,3,9},{260,30,35,9,15,0,4,9},{261,218,67,8,13,1,6,9},{262,99,0,7,16,1,0,9},
{263,227,67,7,13,1,3,9},{264,107,0,7,16,1,0,9},{265,235,67,7,13,1,3,9},{266,115,0,7,16,1,0,9},{267,243,67,7,13,1,3,9},{268,123,0,7,16,1,0,9},
{269,0,81,7,13,1,3,9},{270,40,35,7,15,1,0,9},{271,158,95,10,12,0,4,9},{272,116,121,8,11,0,4,9},{273,169,95,8,12,1,4,9},{274,179,51,7,14,1,1,9},
{275,8,81,7,13,1,3,9},{276,48,35,7,15,1,0,9},{277,16,81,7,13,1,3,9},{278,56,35,7,15,1,0,9},{279,24,81,7,13,1,3,9},{280,64,35,7,15,1,4,9},
{281,32,81,7,13,1,6,9},{282,72,35,7,15,1,0,9},{283,40,81,7,13,1,3,9},{284,131,0,7,16,1,0,9},{285,80,35,7,15,1,3,9},{286,139,0,7,16,1,0,9},
{287,88,35,7,15,1,3,9},{288,147,0,7,16,1,0,9},{289,96,35,7,15,1,3,9},{290,155,0,7,16,1,3,9},{291,11,0,7,17,1,1,9},{292,104,35,7,15,1,0,9},
{293,112,35,9,15,-1,0,9},{294,125,121,9,11,0,4,9},{295,135,121,8,11,0,4,9},{296,122,35,7,15,1,0,9},{297,178,95,8,12,1,3,9},{298,187,51,7,14,1,1,9},
{299,187,95,8,12,1,3,9},{300,130,35,7,15,1,0,9},{301,196,95,8,12,1,3,9},{302,138,35,7,15,1,4,9},{303,163,0,8,16,1,3,9},{304,146,35,7,15,1,0,9},
{305,220,133,8,9,1,6,9},{308,172,0,10,16,0,0,9},{309,154,35,8,15,1,3,9},{310,163,35,8,15,1,4,9},{311,172,35,8,15,1,4,9},{312,229,133,8,9,1,6,9},
{313,181,35,8,15,1,0,9},{314,190,35,9,15,0,0,9},{315,200,35,8,15,1,4,9},{316,209,35,9,15,0,4,9},{317,144,121,8,11,1,4,9},{318,153,121,9,11,0,4,9},
{319,163,121,8,11,1,4,9},{320,172,121,10,11,0,4,9},{321,183,121,9,11,0,4,9},{322,193,121,9,11,0,4,9},{323,219,35,7,15,1,0,9},{324,205,95,7,12,1,3,9},
{325,227,35,7,15,1,4,9},{326,48,81,7,13,1,6,9},{327,235,35,7,15,1,0,9},{328,213,95,7,12,1,3,9},{329,221,95,9,12,-1,3,9},{330,195,51,7,14,1,4,9},
{331,231,95,7,12,1,6,9},{332,243,35,7,15,1,1,9},{333,56,81,7,13,1,3,9},{334,183,0,7,16,1,0,9},{335,64,81,7,13,1,3,9},{336,191,0,7,16,1,0,9},
{337,72,81,7,13,1,3,9},{338,80,81,9,13,0,3,9},{339,68,133,9,10,0,6,9},{340,0,51,8,15,1,0,9},{341,239,95,7,12,1,3,9},{342,9,51,8,15,1,4,9},
{343,90,81,8,13,0,6,9},{344,18,51,8,15,1,0,9},{345,247,95,7,12,1,3,9},{346,199,0,7,16,1,0,9},{347,99,81,7,13,1,3,9},{348,207,0,7,16,1,0,9},
{349,107,81,7,13,1,3,9},{350,215,0,7,16,1,3,9},{351,115,81,7,13,1,6,9},{352,223,0,7,16,1,0,9},{353,123,81,7,13,1,3,9},{354,27,51,9,15,0,4,9},
{355,37,51,8,15,0,4,9},{356,46,51,9,15,0,0,9},{357,131,81,8,13,0,2,9},{358,203,121,9,11,0,4,9},{359,213,121,8,11,0,4,9},{360,231,0,7,16,1,0,9},
{361,140,81,7,13,1,3,9},{362,56,51,7,15,1,1,9},{363,148,81,7,13,1,3,9},{364,239,0,7,16,1,0,9},{365,156,81,7,13,1,3,9},{366,247,0,7,16,1,0,9},
{367,203,51,7,14,1,2,9},{368,0,18,7,16,1,0,9},{369,164,81,7,13,1,3,9},{370,64,51,7,15,1,4,9},{371,172,81,7,13,1,6,9},{372,72,51,9,15,0,0,9},
{373,0,108,9,12,0,3,9},{374,82,51,9,15,0,0,9},{375,92,51,9,15,0,3,9},{376,102,51,9,15,0,0,9},{377,112,51,7,15,1,0,9},{378,10,108,7,12,1,3,9},
{379,120,51,7,15,1,0,9},{380,18,108,7,12,1,3,9},{381,128,51,7,15,1,0,9},{382,26,108,7,12,1,3,9},{383,222,121,6,11,2,4,9},{8211,228,144,7,2,1,9,9},
{8212,236,144,9,2,0,9,9},{8216,126,144,3,5,3,4,9},{8217,130,144,3,5,3,4,9},{8220,134,144,7,5,1,4,9},{8221,95,144,7,6,1,3,9},{8222,103,144,7,6,1,12,9},
{8226,142,144,5,5,2,7,9},{8230,172,144,9,4,0,12,9},{8364,180,81,8,13,0,3,9},{8592,78,133,9,10,0,5,9},{8594,88,133,9,10,0,5,9},{9654,229,121,10,11,0,4,9},
{9660,98,133,11,10,-1,5,9},{10003,238,133,9,9,0,5,9},
};
static const char *const lg__font_rle[] = {
"p%'&$1t#,1&%1,%'6'&,#'$'$'$,1&%1,%'6'%'$'&!1,%'6'&!'&6,!'6'$'6'%1'!'1&'t%'K6t6,%1'!'1%'6!,1&1,%'6'&!'&6,!,6%,#'$1'!'1%K`",
"K%p%e&#;j%;Zo`A$,jA&#;j,$'`e`'$PeZ;`#'jU!Uj'#,jA&#;j,$'`e`'#'jU!Uj'%6j1$'Zf'%;oF%Aj;eK$'`e`'$PK'KP%AoA%;ZoZA%1oU&%Pej1$P",
"K'KP%ZP,j;%;j,$'`e`'$Aeoe;$Fe;jF$PeZ;`$PK'KP$,Z!Z,&%e&#eP$'oP1Fo6$6`,&'`6%UP!PU$P';Z6$P;!;P%6`,&'`6%UP!PU$P;!;P%'`;%PU'K",
"U'$,U6&AeK%UP!PU$,P`P,%1U1$,oK1Fo1%1'&$FZ'A`,#,P`P,$AZ'UF%'`6%UP!PU#6oF1Ko6$FeF%P';Z6$,P`P,$'eAe'&#;`oeA%Fo6$Aj$ZPwv&%Fe",
"$`K!,8,ut%'&#'%K`$ZA%'u#,A,&#,oFeAj6$6;%PZ%'#;ZoZ;$;ZoZ;$;ZoZ;$;ZoZ;$;ZoZ;#PU$UP!PU$UP!PU$UP!PU$UP#;Zo`A$;Zo`A$;Zo`A$;Zo",
"`A$;ZoZA$;ZoZA$;ZoZA#V%'!AqKteA%;ZoZ;$;ZoZ;$Aeoe;$Aeoe;#Fe&#Aeoe;#PU$UP!PU$UP!PU$UP&F`!e!PK$16!1,!PZ&!'oK1Ko'!'oK1Ko'!'o",
"K1Ko'!'oK1Ko'!'oK1Ko'!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!'oP1Fo6!'oP1Fo6!'oP1Fo6!'oP1Fo6!,oK1Fo1!,oK1Fo1!,oK1Fo1!V&%`KtjA$'oK1Ko'!'",
"oK1Ko'!6oF1Ko6!6oF1Ko6!,j`A,$6oF1Ko6!UZ$ZU!UZ$ZU!UZ$ZU&Aj'e&Zoe`ZP!PZ&!Fe$eF!Fe$eF!Fe$eF!Fe$eF!Fe$eF!UZ$ZU!UZ$ZU!UZ$ZU!U",
"Z$ZU!Aj$ZP!Aj$ZP!Aj$ZP!Aj$ZP!Fe$`K!Fe$`K!Fe$`K!V!P[K%`KtjA$Fe$eF!Fe$eF!K`$ZA!K`$ZA#'UjoK#K`$ZA!UZ$ZU!UZ$ZU!UZ$ZU&'j`j'$6",
"o,!6oP!PZ&!V$V!V$V!V$V!V$V!V$V!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!PZ%'!PZ%'!PZ%'!PZ%'!V%'!V%'!V%'!V!AKa%`KtjA$V$V!V$V!Fe&!FetAjF!Fe",
"&!UZ$ZU!UZ$ZU!UZ$ZU&!,Zoj;#PZ$`P!PZ&!V$V!V$V!V$V!V$V!V$V!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!PZ&!PZ&!PZ&!PZ&!V&!V&!V&!V$P`%`KtjA$V$V",
"!V$V!,j`A,$,j`A,&$KZ!,j`A,$UZ$ZU!UZ$ZU!UZ$ZU&$ePoA!V$ZP!Fe$V!V$V!V$V!V$V!V$V!V$V!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!PZ&!PZ&!PZ&!PZ&",
"!V!P[K!V!P[K!V!P[K!Fe$ZP%`KtjA$V$V!V$V#'UjoK$'UjoK#ZP$PZ#'UjoK#UZ$ZU!UZ$ZU!UZ$ZU&$e!UZ!PZ$`P!,oK,Ao6!V$V!V$V!V$V!V$V!V$V",
"!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!PZ&!PZ&!PZ&!PZ&!V!AKa!V!AKa!V!AKa!,oF,Ao1!3eU2'&#jA$V$V!V$V&AjF&AjF!AoA,Aj;&AjF!UZ$ZU!UZ$ZU!UZ$",
"ZU&UF!e!K`!;o,!1oP#;eoeF#V$V!V$V!V$V!V$V!V$V!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!PZ&!PZ&!PZ&!PZ&!V$P`!V$P`!V$P`#A`o`F#Zso;!',%jA$V$V",
"!V$V&!KZ&!KZ#Fjo`F&#KZ!UZ$ZU!UZ$ZU!UZ$ZU&Aj;e;jA!'`oaZP$FZ,$Fe$eF!Fe$eF!Fe$eF!Fe$eF!Fe$eF!Fe$eF!Fe$eF!Fe$eF!Fe$eF!Ae$V!A",
"e$V!Ae$V!Ae$V!Fe$ZP!Fe$ZP!Fe$ZP%'&!AZ&'o6#,o1$Fe$eF!Fe$eF!ZP$PZ!ZP$PZ$KU,$ZP$PZ!Fe$eF!Fe$eF!Fe$eF&'FeoeF'$1;'ZP$'Ao'#,oF",
",Fo,!,oF,Fo,!,oF,Fo,!,oF,Fo,!,oF,Fo,!,oF,Fo,!,oF,Fo,!,oF,Fo,!,oF,Fo,!'oK,Ao6!'oK,Ao6!'oK,Ao6!'oK,Ao6!,oF,Ao1!,oF,Ao1!,oF",
",Ao1%FA&jA&![61U`%,oF,Fo,!,oF,Fo,!;oA,Aj;!;oA,Aj;$'Fj'#;oA,Aj;!,oF,Fo,!,oF,Fo,!,oF,Fo,&$e'&$jA$UeU%A`o`A$A`o`A$A`o`A$A`o",
"`A$A`o`A$A`o`A$A`o`A$A`o`A$A`o`A$;ZoeF$;ZoeF$;ZoeF$;ZoeF$A`o`F$A`o`F$A`o`F%Aj1&Fje&,PkP1&A`o`A$A`o`A$F`o`F$F`o`F%ZeU%F`o",
"`F$A`o`A$A`o`A$A`o`A&%e&;L`j'$'&$'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&!K;&$'&$'&%'&#'&#'&#'&!'&$'&#'&#'&#'t!'&",
"AZUF,xxxt&#'6!,1&6,!,6t6;&'t$AG;!;6&!;GA%'K6&';6!'F'!6A,&'F'%'&!,1t!1,&!'6'&#,#'&'$'&!K`K&,6&%2&6'%(#'$,1&%1,%'6'%'$'%,#",
"'&#1,t%ZP,j;$,eo1!1oe,&$eK$Feo`F'$;ZoZ;#jeZK!Ke&!KZej%6oU%6jeA!1o1!AeoA%1o1#KeojP%,jA&%;j,&'`e`'&PeZ;`%'jU!Uj'%,Z!Z,%'eK",
"&#1j6%Zf,$e`!Ko,#,jA&#;j,$'`e`'#'jU!Uj'#PeZ;`&!;j,t$AZ'UF$,jZ,$,Zj,&!6o,#AeF'6ZU#1oF1Ko1!jA$,o6&#Aj&1'%V$1o1$U`%1o1!6o;,",
"6jF%6`,&#'`6&!UP!PU&P';Z6&P;!;P&'eAe'&1`1&!ZA%KZ'FZ'#G!6U'$6`,&'`6%UP!PU$P;!;P$P';Z6&'`6u#Pe'&'eP&!UZ$ZA$,j'!K`$`K!jA%ZU",
"&#Aj!19,%`K$1o1$K`%1o1!K`$(vt%,A,wt&PU$UP#oF&#Fo&'oA#'o,#1'e;!`K$K`!jA%Ao'&!Aj!ZrK%ZP$1o1$V%1o1!6oK6'&!FjK&#FjK&#FjK&#Fj",
"K&#FjK&#FjK%FnZ!FnZ!FnZ!FnZ!AnA!AnA!AnA!AnA!Pj;#Q!6j1$,j6tUZ$ZU!,o6&#6o,%Aj'#;e#`e`eA!`K$K`!jA%'jA&!Aj&`K%V$1o1$`K%1o1#F",
"okK'%`e`&#`e`&#`e`&#`e`&#`e`&#`e`%Ke91!Ke91!Ke91!Ke91!,6AoA6,!,6AoA6,!,6AoA6,!,6AoA6,!UoU#V#`U$P`t!UZ$ZU!1o,&#,o1%`P$;e!",
"1j#jA!`K$K`!jA&P`&!Aj&`K$1eA$1o1$PZ,$'6'!;j,!6jA$'oAo'&'oAo'&'oAo'&'oAo'&'oAo'&'oAo'$Ke&!Ke&!Ke&!Ke&$1o1&1o1&1o1&1o1$UZj",
"'!V#6o,!,o6t!UZ$ZU!1o,&#,o1$1o6$;e!A`#`A!`K$K`!jA&6o1&Aj&`K!eoj;%1o1$'Zpe&PZ$UZ$6j'j;&6j'j;&6j'j;&6j'j;&6j'j;&6j'j;$Ke&!",
"Ke&!Ke&!Ke&$1o1&1o1&1o1&1o1$UP`A!V$`P!K`t#UZ$ZU!1o,&#,o1$Ke%;e!A`#`A!`K$K`!jA&!eK&Aj&`K!2Aj;$1o1$Ke;2#'#Fj1!'eF$KZ!ZP&KZ",
"!ZP&KZ!ZP&KZ!ZP&KZ!ZP&KZ!ZP$Ke>'!Ke>'!Ke>'!Ke>'$1o1&1o1&1o1&1o1$VFZ!V$;o1j;t#UZ$ZU!,o1&#1o,$jF%;e!,o(j,!PZ$ZU!jA&!Fj&Aj&",
"`K%ZP$1o1$`K%1o1#UoejP%`F!F`&`F!F`&`F!F`&`F!F`&`F!F`&`F!F`$Koh6!Koh6!Koh6!Koh6$1o1&1o1&1o1&1o1$V,o,V%gt$UZ$ZU#oA&#Ao$;o,",
"%6j#KkK#1oF,Fo;!jA&!,o;%Aj&`K%ZP$1o1$ZP%1o1$,;UoA#'o6'6o'$'o6'6o'$'o6'6o'$'o6'6o'$'o6'6o'$'o6'6o'#Ke&!Ke&!Ke&!Ke&$1o1&1o",
"1&1o1&1o1$V!ZFV%AoAt$UZ$ZU#Z`&#`Z$UZ&'o6#(%A`pK#jA&#ZU%Aj&eF%`K$1o1$P`%1o1&!UZ#;s;$;s;$;s;$;s;$;s;$;s;#Ke&!Ke&!Ke&!Ke&$1",
"o1&1o1&1o1&1o1$V!A`PU%1o1t$Fe$eF#1oK&Ko1#'o;&!`Ut,jK#jA&#;o'$Aj%1o,%ZP$1o1$P`%1o1!P`'!'`K#PZ)ZP$PZ)ZP$PZ)ZP$PZ)ZP$PZ)ZP$",
"PZ)ZP#Ke&!Ke&!Ke&!Ke&$1o1&1o1&1o1&1o1$V!'oZU%1o1t$,oF,Fo,$6je,!,ej6$Fj'&!,ejPA1&#Ao1!jZKA&'jF!AKZj!;KUjZ&AjU;!1o1!;UjK%1",
"o1!,eoeo`,#eK$Fe$eK$Fe$eK$Fe$eK$Fe$eK$Fe$eK$Fe#Ke5!Ke5!Ke5!Ke5!'1AoA1'!'1AoA1'!'1AoA1'!'1AoA1'!V#UoU%1o1t%A`o`A&,F1!1F,%",
"G&$'APZA&$PF!PVF&!G!FVP!FZP6&#1KA!,U,!AP;&,U,$,;,$,o6$6o,!,o6$6o,!,o6$6o,!,o6$6o,!,o6$6o,!,o6$6o,!Ks`!Ks`!Ks`!Ks`!AsA!As",
"A!AsA!AsA!V#;oU%1o1t&!'xxxut#'Z6&F6!6F&1'!'1&!FjK&6,!,6$1'#6&'%FnZ#1,!'6'$F`;%P1!6K%1P,&'6'&B&$,#'$1'!'1#AnA%'&#6`K$PU$F",
"e'!KZ&%2&%2%'j6&!KlP&%1,#Pj;#Q#6,!,6$U]Ut!ZU&'oU!Uo'%PK'KP&!`e`&Fe;jF$FU'AZ%6oK$Ke91#;j;`P$FjAj;$AeKe6%FoA%'`e`'$Ke`U&!P",
"eZ;`$PK'KP#,6AoA6,$AoA&6jFeP#PZ#'jK#K`&$6j1&$6j1%'o6&!,7`U&$;j,#UoU#V#Fe;jF$;C;t';'&!,'!',&,P`P,&'oAo'&FeF%'K`P1%,U6$Ke&",
"$;eP$';,!,;%';'&!1&UP!PU#1P,'P6&P';Z6$,P`P,%1o1&1U1&61!';'!PZ#Fj'#K`$'6'!'ZA&$'ZA&'o6&%ZU&#'`6$UZj'!V$FeFt&!'6'$'6'!'6'$",
"'6't$6j'j;u&#Ket&!16!1,$16!1,$16!1,ut&#1o1t!19,$PZ!'jF$K`$`Zu!'o6&%ZUt$UP`A!Vt;ZoZ;&$eK$Ke$eK$Ke&FjK&!KZ!ZP$PkeU1#FnZ!Fn",
"Z!Ke>'!FnZ#Zoe`ZP#Zoe`ZP#Zoe`ZP!PZ$ZP$PU&!AnA!AnA$1o1$AnA!ZrK$P`6Pj'$K`#Fj,#'j6&!KlP&'o6&%ZU&Pj;#Q!VFZ!V!Pj;#Q!'oK1Ko'&#",
"Fj$eK$Fj$eK&`e`&!`F!F`$P`6;Zj,!Ke91!Ke91!Koh6!Ke91!6o,!6oP!6o,!6oP!6o,!6oP!PZ$ZP$PZ&!,6AoA6,!,6AoA6,$1o1$,6AoA6,&`K$Pojo",
"U%K`!1oA$'o6&!,7`U&'o6&%ZU&UoU#V!V,o,V!UoU#V!Fe$eF&#,o;!1o,$,o;!1o,%'oAo'%'o6'6o'#PZ#'jF!Ke&!Ke&!Ke&!Ke&!PZ$`P!PZ$`P!PZ$",
"`P!PZ$ZP$PZ!61&1o1&1o1&1o1&1o1&#`K$PZ!Ao,$Koej`%'o6&%ZU&'o6&%ZU&UZj'!V!V!ZFV!UZj'!V!V$V&$ZU!P`&ZU!P`&6j'j;%;s;#PZ$V!Ke&!",
"Ke&!Ke&!Ke&!V$ZP!V$ZP!V$ZP!PZ$ZP$PZ`eo`%1o1&1o1&1o1&1o1&#`K$PZ#eP$Ke;Pj,$'o6&%ZU&'o6&%ZU&UP`A!V!V!A`PU!UP`A!V!V$V&$;o,jA",
"&;o,jA&KZ!ZP%PZ)ZP#PZ$V!Ke>'!Ke>'!Ke&!Ke>'!PZ$`P!PZ$`P!PZ$`P!P`8`P$Pj1!1o;$1o1&1o1&1o1&1o1&#`K$PZ#Ao,#K`#`Z$'o6&%ZU&'o6&",
"%V&VFZ!V!V!'oZU!VFZ!V!V$V&%eUo'&!eUo'&`F!F`%eK$Fe#PZ$V!Koh6!Koh6!Ke5!Koh6!;o,!1oP!;o,!1oP!;o,!1oP!PoloP$P`$`P$1o1&1o1$'1",
"AoA1'$1o1&#`K$PZ$eU#K`#1oA#'o6&%ZU&'oF4'%Aj;2'!V,o,V!V#UoU!V,o,V!V$V&%KoU&#KoU&'o6'6o'#,o6$1o,!PZ$V!Ke&!Ke&!Ks`!Ke&!'`oa",
"ZP!'`oaZP!'`oaZP!PZ$ZP$PZ$ZU$1o1&1o1$AsA$1o1&#`K$PZ$Ao,!K`$Kj,!'o6&%ZU&'so6%'Pjp6!V!ZFV!V#;oU!V!ZFV!V$V&%1o6&#1o6&;s;&$U",
"K#PZ$V!Ke&!KetKP#Ke&$1;'ZP$1;'ZP$1;'ZP!PZ$ZP$PZ$ZU$1o1&1o1&UK&!1o1&#eFu!'o6&%ZUu%V!A`PU&%V!A`PU!Fe$eF&%Fe&$Fe&!PZ)ZP&#,o",
",#PZ$jF!Ke&!Ke&%'o6#Ket!jA&!jA&!jA!PZ$ZP$PZ$ZU$1o1&1o1%,o,&!1o1&!1o,&!KA&#KA%'o6&%V&$1P,&#;K%V!'oZU%KA$V!'oZU!,oF,Fo,&%e",
"K&$eK&!eK$Fe&$UjU!P`1;Uj,!Ke5!Ke5&PjZ!Ke5#;L`j'#;L`j'#;L`j'!PZ$ZP$PZ$ZU!'1AoA1'!'1AoA1'$UjU$'1AoA1'!;KUjZ&!Aj,&!Aj,%'oF4",
"'%Aj;2'%`U&#,oA%V#UoU$Aj,$V#UoU#A`o`A&%,Z,&#,Z,&,o6$6o,&%'!PpjZ1#Ks`!Ks`&#'!Ks`#AZUF,$AZUF,$AZUF,#PZ$ZP$PZ$ZU!AsA!AsA&'$",
"AsA!FZP6&#K6&#K6&'so6%'Pjp6$1P'&#AF&V#;oU$K6%V#;oU%'xwu&%1,$Pl`F%6,!,6$,nk,%K6&!6,!,6%U]U#PU$UP%'6'&#'6'&#A`A&!'$'&#1,&'",
"&6,!,6&;&'Fej!PK$,F1$UP&$U]U%K^'#U]U#Pj;#Q$1F1t&#'t&$'t%;j,$P`7KjF$Fe;jF$'7AoA7'%jA&!Fe;jF%;C;#UZ$ZU$'`e`'&'`e`'&AjAjA%'",
"jU!Uj'&;j,%AoA%Fe;jF&j&Kq!Q$e;`,#V&$;C;%;D$;C;#UoZ#V#'`;e'#,ZjK$PF$AeoZ'&;ZoZ;%AeoeAt!'`6%PZ$Pe%FeF&#1o1%'7jP71%FeFt#UZ$",
"ZU$UP!PU&UP!PU&;,!,;&P;!;P&'`6&1U1&FeF&,j1%jq!Q#'`!U1#V!6,u&%UZo,!V#,Z!Z,#Q,e1!;`$'jA1`U%1oF,Fo1#;oF1Fo;t&#PZ$Fjt%1o1%6s",
"o`t!PU$UP!UZ$ZUu!'6'$'6'ut&!PjpZ'#r!Q$A`K$VfoZ&FjK%FnZ!AnA!UPZK!V$F`F$`;!ZA'e,$6o'!;U%KZ$ZK#P`$`P&$Pl`F$PZ#,eU#Pl`F&!1o1",
"&#jA%,nk,!UZ$ZU!UZ$ZU!PK!6j;!KP!6j1$,j6#eK$Ke#6j1$,j6!PnK!PnK!PnK!6o1j,eF#Zq!Q$,;'$Uj1!1o6%`e`%Ke91!,6AoA6,!V6j!V!,1$1,!",
"Q,e1UF&eF&#`K$K`#Fe$eF&$P`7KjF#Pjfo`,#P`7KjF&1o1&#jA%'7AoA7'!UZ$ZU!UZ$ZU!KU!AjF!UK#`U$P`$Fj$eK$`U$P`#18;jF!18;jF!18;jF!P",
"Z!j!6;#6ep!Q#ZoeoZ#UZ$`K$'oAo'$Ke&$1o1$V!e;V!UZ$ZU!,ZeK;Z&!Ao1&!`K!6!K`#'`ZAZ`'&$PZ$Pe#P`;Ko,$PZ$Pe&1o1&#jA&#1o1%UZ$ZU!U",
"Z$ZU!A`!KZK!ZA#6o,!,o6$,o;!1o,$6o,!,o6&!Ke'&Ke'&Ke'!V!j&#;o!Q!;`'!,o6!V$ZP$6j'j;$Ke&$1o1$V!F`PU!UZ$ZU%,e,&Uj`e'!6F#`K;o6",
"K`$AojoA&%PZ$Fj#PZ#eK$PZ$Fj&1o1&#jA&#1o1%UZ$ZU!UZ$ZU!6j!UFU!e6$`P!K`&ZU!P`&`P!K`&!,o;&,o;&,o;#V!j&#1o!Q&!ZP!V$ZP$KZ!ZP$K",
"e>'$1o1$V!'oZU!UZ$ZU%ZAFeU,!;j'![,jA#`K!6!K`#Kj6!6jK&$PZ#,eU#PZ#Fj'#PZ#,eU&1o1&#jA&#1o1%UZ$ZU!Fe$eK!1o!`6`!o,$;o1j;&;o,j",
"A&;o1j;&!UZ&!UZ&!UZ$K`!j!G%1o!Q!'UjqP!U`$eF$`F!F`$Koh6$1o1$V#UoU!UZ$ZU$AZ1j1PK!PZ#'ejP$`K$K`#eK$Ke&$Pjfo`,#PZ#'jF#Pjfo`,",
"&1o1&#[62%1o1%UZ$ZU!,oF,Fo;!'o,j!j'o'%g&#eUo'&!g&!6o1&6o1&6o1$,oFj;j;%1o!Q!U`1(ZP!UeA,Ao,#'o6'6o'#Ke&$1o1$V#1oU!UZ$ZU#,e",
"'AZ!;`!K`$Zo;$PZ$ZU#eP$Pe&$P`;Ko,$PZ$Kj#P`;Ko,&!1o1&#1`pZ%1o1%UZ$ZU#A`pK$j6e!e6j&AoA&#KoU&#AoA&!eP&!eP&!eP&;`ojK&1o!Q!jF",
"$eP!VPoeF$;s;#Ke&$1o1$V$`U!Fe$eF#Z;!1e'KP!1oA1ZeFj,#1oA,AoA#KjA,AjK&$PZ#eKt#PZ#eK&#eA&$Q'&1o1%Fe$eF%Q%`KZ!ZF`&1o1&#1o6&#",
"1o1&Fj'&Fj'&Fj'&#j&#1o!Q!P`1,FjP!V!'&PZ)ZP#Ke&$1o1&$j;!,oF,Fo'!FU$Pj`1#FeoZ,!U`$A`ojF%KjojK&%PZ#Fj'&KA%PZ#Fj'&!,ZU&#'Ke&",
"1o1%,oF,Fo,$'j6%UZP!PZU&1o1&#Fe&$1o1%'jA&'jA&'jA&$j&#1o!Q!,ZojKZP!V&#eK$Fe#Ke5!'1AoA1'$1Kej'#A`o`At&'t'&$'t!PZ#'jF%Aj,%P",
"Z#'jF&1`e;&#`eK&1o1&A`o`A&PjZ$PjF!KjK&1o1&#eK&$1o1%Pe64!Pe64!Pe64%'&#,Z!FA$'&A;&!,o6$6o,!Ks`!AsA$6UA'&'v&%PZ$Kj%K6&PZ$Kj",
"&!'&%'&#1o1&#'&%'$Fo;!AoA&1o1&!,Z,&$1o1%ZsZ!ZsZ!ZsZxxw%'t!1,#6Z,&#'Z;%;`F%1P;!P$F;!6K%';,%;Z'&#'Z6%A`A%F6!6Ft!;Z'&#'Z6%A",
"`A%6P6'K$F6!6F$;Z'&#'Z6%A`A%F6!6F$,4$K;!1P%,;'&#'UA%;`F&'K1&%;Zo`A$;ZoZA$;ZoZ;$Aeoe;%;Zo`Kj,$P`'&!UZ%;jAeF$ZFZ`Z$jZ!Po'#",
"PjeoZ'$UZ&#ZU%AjAjA#'oU!Uo'#1j<Z%UZ&#ZU%AjAjA$`AaU#'oU!Uo'$UZ&#ZU%AjAjA#'oU!Uo'#`m$6eKeA$ZoeoZ&!PZ%;jAeK%;oP&$'oP1Fo6!,o",
"K1Fo1!'oK1Ko'!6oF1Ko6#,oK1PoK&;,&';'%;1!,;'#,#1%,'!',#1o6!'jA$';'&';'%;,!,;$,'!',$'Zoe;%';'&';'%;,!,;$,#1%,'!',%';'&';'%",
";,!,;$,'!',t#';'$;`'!,o6%';'%61!,;'%1&%Aj$ZP!Fe$`K!Fe$eF!K`$ZA#Fe#;oP%,;'&,;'&,;'&,;'&,;'$P`$6;$';1&';1&';1&';1$1jZ`Z&'6",
"'&'6'&'6'&'6'&'6'$,1$1,!,1$1,!,1$1,!,1$1,$,;'&,;'&$ZP%';,&';,&';,&$PZ%'!V%'!V$V!Fe&#V#ejU$ZoeoZ$ZoeoZ$ZoeoZ$ZoeoZ$ZoeoZ#",
"V&#Pj`oU$Pj`oU$Pj`oU$Pj`oU#,1!,jA$PlP$PlP$PlP$PlP$PlP#UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU#ZoeoZ$ZoeoZ#'UjqP$PjeoZ'#PjeoZ'#PjeoZ'&!PZ",
"&!V&!V$V!,j`A,%V!F`V#;`'!,o6!;`'!,o6!;`'!,o6!;`'!,o6!;`'!,o6!V&!1j,!,jA!1j,!,jA!1j,!,jA!1j,!,jA#';Gj'!1o1!1o1!1o1!1o1!1o",
"1!1o1!1o1!1o1!1o1!1o1!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!;`'!,o6!;`'!,o6!U`1(ZP#1o6!'jA!1o6!'jA!1o6!'jA&!PZ&!V!P[K!V$V#'UjoK$V'j1V&",
"#ZP&!ZP&!ZP&!ZP&!ZP!Ke$G!PU$V!PU$V!PU$V!PU$V!,kUeoF!PZ$ZP!PZ$ZP!PZ$ZP!PZ$ZP!PZ$ZP!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU&!ZP&!ZP!jF$eP#",
"P`$6;!P`$6;!P`$6;&!PZ&!V!AKa!V$V&AjF#VPU!V#'UjqP!'UjqP!'UjqP!'UjqP!'UjqP!1oF,;j;!ZjbjZ!ZjbjZ!ZjbjZ!ZjbjZ!Ue'!'eZ!ZP$PZ!Z",
"P$PZ!ZP$PZ!ZP$PZ!ZP$PZ!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!'UjqP!'UjqP!P`1,FjP#V&!V&!Vt!PZ&!V$P`!V$V&!KZ#U`j,!V#U`1(ZP!U`1(ZP!U`1(ZP",
"!U`1(ZP!U`1(ZP#;eoeK#ZU4,!ZU4,!ZU4,!ZU4,!eK$Ke!ZP$PZ!ZP$PZ!ZP$PZ!ZP$PZ!ZP$PZ!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!U`1(ZP!U`1(ZP!,ZojK",
"ZP#V&!V&!Vt!Ae$V!Fe$ZP!Fe$eF!ZP$PZ#PoK#eF#jF$eP!jF$eP!jF$eP!jF$eP!jF$eP$FZ,$KZ$(!KZ$(!KZ$(!KZ$(!`U$UZ!F`$`F!F`$`F!F`$`F!",
"F`$`F!F`$`F!Fe$eF!Fe$eF!Fe$eF!Fe$eF!jF$eP!jF$eP$'!,e1#Fe$G!Fe$G!Fe$G&!'oK,Ao6!,oF,Ao1!,oF,Fo,!;oA,Aj;#AoK,Fo,#P`1,FjP!P`",
"1,FjP!P`1,FjP!P`1,FjP!P`1,FjP$'Aj'#,oA'6j;!,oA'6j;!,oA'6j;!,oA'6j;!FoF,FoA!,oF1Fo,!,oF1Fo,!,oF1Fo,!,oF1Fo,!,oF1Fo,!,oF,F",
"o'!,oF,Fo'!,oF,Fo'!,oF,Fo'!P`1,FjP!P`1,FjP&F`$,oF,;j;!,oF,;j;!,oF,;j;&#;ZoeF$A`o`F$A`o`A$F`o`F$eUeo`A$,ZojKZP!,ZojKZP!,Z",
"ojKZP!,ZojKZP!,ZojKZP$UeU%A`oeK$A`oeK$A`oeK$A`oeK$KjojF$AZoZ;$AZoZ;$AZoZ;$AZoZ;$AZoZ;$A`o`A$A`o`A$A`o`A$A`o`A#,ZojKZP!,Z",
"ojKZP&,`e;#AZoeK$AZoeK$AZoeKt'&#'&#'&#'%1K#'&#'&#'&#'&#'&#'&#'&$'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&#'&!'&#'t!'&'&#'&#",
"'xwu&!KP!FU'#,3,$P6!6P%,P,&';'%PK!KP#,1!61%,3,$P6!6P%;U'PFt#'6!16&!'Z6%A`A&1;,%PK!KPt#6P6'K$,3,$P6!6P%;U'PF!,1$1,t6j6&%P",
"n;!Fn;&ZU&$'&';'!'k`Kt!'`ej,$ele$;eKe;%FoF%Pj`jP$'g'#PZ`eo`$ele$;eKe;$1j1F`'#AejKlP#;oKeje1%ZU%AjAjA#'ej`j`'#'g'&#6j$`Aa",
"U$ele$;eKe;$1j1F`'!UZ$ZU$;Zo`A#6o6#AZoZ;#18Pj1!K`9'%;o1&!FeoeF$UoU#<KjUt!';,t$';'&!1%1o,!,o1$';'$Pj1!1o;t!';'%2!;,#'oK1`",
"o71#;oF!'`P$';'%;,!,;#Ae#'Z;$';'&!K6;`$,#1t$';'%2!;,#UZ$ZU#'oP1Fo6!6o6!1oF1Ko,%;j6#K`&%eP&!AjK1KoF#;`;&Ao't';,&';1&';1&'",
";1$PZ$ZP$';1$P`$`P$'6'&'6'&'6'$;o'!Fo&;o,#Fe$1;,&1;,$Fj1&#1;,&!jA6F#,1$1,!,1$1,!,1$1,!,1$1,!UZ$ZU#Aj$ZP!1o1!PZ$`K$6j;$K`",
"&$Aj'&!`U$U`t1o1&%PjeoZ'#Pj`oU$Pj`oU$Pj`oU#UjbjU#Pj`oU#PZ$ZU#PlP$PlP$PlP#Aj#;o&;o$,6!'ej`j`'!'ej`j`'!,`ojZ;#'ej`j`'!'7jP",
"71!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!'UZ-'!'!1o1!,'$ZU$`jK,#Koe`K,$'jZUK1%oA$Ao&%'Pj&%1o6!'jA!1j,!,jA!1j,!,jA!1j,!,jA!UZ4,!1",
"j,!,jA!PZ$ZU!1o1!1o1!1o1!1o1!1o1!1o1!Aj#;o7'#;o&!Ae#'Z;!Ae#'Z;$1;Uo;!Ae#'Z;!6so`!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!Pkf6$1o1&",
",jA$6AZj'!1C`o'#FoZAZo6$`U$Ze&$Zjo;&%P`$6;!PU$V!PU$V!PU$V!F`$(!PU$V!PZ$ZU!PZ$ZP!PZ$ZP!PZ$ZP!Aj#;ok6#;o&!Fj1&Fj1&2$`P!Fj1",
"&$jA%UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!Fe$eK#PZ&!,o,&`Z&!'jA&!jF#`Z$Z`$;oU;UoF$1'$jP'tV&!ZjbjZ!ZjbjZ!ZjbjZ!,oA'6jA!ZjbjZ!PZ$ZU!ZP$",
"PZ!ZP$PZ!ZP$PZ!Aj#;o&;o&!,`ojZ;#,`ojZ;#Aj6'6jA!,`ojZ;&jA%UZ$ZU!UZ$ZU!UZ$ZU!UZ$ZU!,oF,Fo6!FejV1$'`'%Uj,&#ZU&!UZ#oA$Ao%;UZ",
"`j'#1o;$Z;t!V&!ZU4,!ZU4,!ZU4,#A`pP#ZU4,!PZ$ZU!ZP$PZ!ZP$PZ!ZP$PZ!Aj#;o&;o&$1;Uo;$1;Uo;!'PjojP%1;Uo;%jA%UZ$ZU!UZ$ZU!UZ$ZU!",
"UZ$ZU#A`pK#1Z`<,&%Pj1$ZP$eF!V$`K#`P$P`&!,jF$Ao,t&!Fe$G!KZ$(!KZ$(!KZ$(%PK$KZ$(&%F`$`F!F`$`F!F`$`F!;o'!Aot$2$`P!2$`P$A`1$2",
"$`P%jA%Fe$eF!Fe$eF!Fe$eF!Fe$eF%ZK%Ae$V!6U6#Kj1%Aj;,Ko'!;oA,Fo,#FjF,FjF&!Ue%Pj$,UAt!,oF,;j;!,oA'6j;!,oA'6j;!,oA'6j;$'o,$,",
"oA'6j;%KA$,oF1Fo,!,oF1Fo,!,oF1Fo,!'oK1`o2,#,P1&Aj6'6jA!Aj6'6jA$';o,#Aj6'6jA%jA%,oF,Fo'!,oF,Fo'!,oF,Fo'!,oF,Fo'$,o,%'oK,A",
"o6!FoF!FoF4!'KeoZ;$F`o`A%KjojK&!1o;%ZU$1oUt#AZoeK$A`oeK$A`oeK$A`oeK&UjU$A`oeK%Aj,%AZoZ;$AZoZ;$AZoZ;$FkPqP#[&!'UjojP#'Ujo",
"jP%PeZ$'UjojP&UZ62#A`o`A$A`o`A$A`o`A$A`o`A&UjU%;ZoeF$'#Zsj%'&#'&$'&$a&;1%'t&'&#'&#'&#'&%'&'&!K6&#'&#'&#'&!(&!1P,&%,&#,&!",
"'&$,&#1ZpZ%'&#'&#'&#'&%'&!'xxt$eA!PU$UP!PUt!UP$16!1,$'K6%,1!61&16!1,!'6'$'6'!'6't,1$1,%,6&!'&!,Z6&%UF&1`K&;A!1K'$1P;!P&#",
"KZ1j&#ZK$,PA!K'$,4%B!'Z&!'Z;$Q!FU'#'`F&!,1!61&#PK$AU';Z,&$jA!UZ$ZU!PZt!ZP#Zoe`ZP$1oU%PZ`eoZ'!'Zoe`ZP#eK$Ke#KoK#F`o`A$PZ$",
"ZP%`o6$;eoeF&Fj,&#Ke'%1jF`P%`j!Fo6$ZFZ`Z&#K`;e%1VjeU#QU`e%Um,$,eP`K&!UZ%'`f,$Pe'&!PZ`eo`&Fe'%Pej6&%jA!UZ$ZU!PZ!61&16!ZP!",
"6o,!6oP%1'%Pj6!,o;!;o,!6jP#Fj$eK#,F,!1oF1Fo1#PZ$ZP%1A'#1oK1Fo;&61&#;,&61!';'$',#1%,#1&';'K`6K%,Ge`F#,'!1't&6,&!';'&';,%;",
",&#Po1!1o6%;,&!;1tjA!UZ$ZU!PZ`eoZ'!'Zoe`ZP!PZ$`P!,8,%P`$ZP!PZ$`P#,o;!1o,&!P`$UA#PZ$ZP&%Ke$`P#,8,%,8,%,8,%,8,%,1!61%;lP`&",
"!6;'ZP#,8,%,8,%,8,%,1!61$,1!61&,1!61$Pe$`P!'6!16$'6!16tjA!UZ$ZU!Pj6!,o;!;o,!6jP!V$ZP!AqK%PZ$V!V$ZP$ZU!P`$,o,!P`&#PZ$ZP%F",
"j$PZ$eA#AqK%AqK%AqK%AqK%PZ`eo`$ZU!'e`%'eoaZP#AqK%AqK%AqK%PZ`eo`#PZ`eo`%PZ`eo`#PZ$ZP!;oKeje1!;oKeje1&$jA!UZ$ZU!P`$ZP!PZ$`",
"P!PZ$`P%`K%PZ$V!V$ZP$;o,jA$,o,!1o;&!PZ$ZP$6Zj$PZ'A['&`K&#`K&#`K&#`K%Pj1!1o;#o;#P`%;o,!1oP&`K&#`K&#`K%Pj1!1o;!Pj1!1o;$Pj1",
"!1o;!PZ$ZP!;oF!'`P!;oF!'`P&$jA!UZ$ZU!PZ$V!V$ZP!;o,!1oP%`K%Pe$`K!K`$eP%eUo'$1o1!UkaA$Pe$jP#KoZF$PZ,eoK&!`K&#`K&#`K&#`K%P`",
"$`P!'o6#K`%PZ$`P&`K&#`K&#`K%P`$`P!P`$`P$P`$`P!PZ$ZP!;o,#Fe!;o,#Fe#',%jA!UZ$ZU!PZ$V!V$ZP!'`oaZP%`K%PeF,Ao1!1o;,FeP%KoU%1o",
"1!APoFA1$PjF,FjP!'oF&PZ#1eP&`K&#`K&#`K&#`K%PZ$ZU!'o6#K`%UZ$ZP&`K&#`K&#`K%PZ$ZU!PZ$ZU$PZ$ZU!PZ$ZP!;o$,6!;o$,6#'o6#,o1!Fe$",
"eF!Pe$`K!K`$eP$1;'ZP%`K%PZKkK$KkKZP%1o6%1o1#;o'&PZ`o[P!1o1&PZ$Fe&`K&#`K&#`K&#`K%PZ$ZU#jA#U`%Ke$jP&`K&#`K&#`K%PZ$ZU!PZ$ZU",
"$PZ$ZU!PZ$ZP!;o&!;o&$[61U`#,oF,Fo,!PjF,Ao1!1oA,FjP&!jA%`K%PZ!'&#'!ZP%Fe&6o6!'eU&!PZ!'%'oF&PZ$Pe&`K&#`K&#`K&#`K%PZ$ZU#U`1",
"6e`%1oA,FjP&`K&#`K&#`K%PZ$ZU!PZ$ZU$PZ$ZU&'j;!;o&!;o&$,PkP1$A`o`A#PZKojK$KjoKZP#;L`j'!3eU2'!PZt!ZP%eK&6o6!Ze64#PZ&#Ko`Q#P",
"Z'6KoK#3eU2'!3eU2'!3eU2'!3eU2'!PZ$ZU#1ZoZP`&KkKZP#3eU2'!3eU2'!3eU2'!PZ$ZU!PZ$ZU$PZ$ZU$1Pej#;o&!;ot!'&#'&#'&#'&AZUF,#Zso;",
"!FKt!KF$,Z,&1Z1!eso#AK&$;KUZ'!PZ,o`F$Zso;!Zso;!Zso;!Zso;!PZ$ZU%'t'&Zso;!Zso;!Zso;!PZ$ZU!PZ$ZU$PZ$ZU$6U;'#;o&!;oxwu&!A`A&",
"#'Z6%,P,%PK!KP&`1!;U$'UjU&#6j6#Znj1%FjK%PlP6#PkeU1#FnZ!Kn`!PZ$ZP!AnA!PU$Fe'!'j6&!ej1!1je!Pj;#Q!PlZA'#Pl`F$,nk,!,j6$6j,!P",
"K!6j;!KP!'eK$Fe'!6j1$,j6!PnK%'Pl!PU&%AjAjA&!ZU&FoF%'g'&o'!KF#1eaU&#eU$`U8Ko,%`e`%P`6;Po,!P`6;Zj,!Ke91!P`:!PZ$ZP!,6AoA6,!",
"PZ#'jK#'o6&!j`K!KZj!UoU#V!P`7FjK#P`7KjF#'7AoA7'#eF$Fe#KU!AjF!UK#Fo,!'jA$`U$P`#18;jF%KoA7!PZ&%;,!,;&';'&!1&!';'&1e#Z6#UP!",
"V&!Fj,$`P$[%'oAo'$PZ$`K!PZ#'jF!Ke&!PZ&!PZ$ZP$1o1$PZ#Fj'#'o6&!jK`!`Fj!UZj'!V!PZ$Aj#PZ$Pe&1o1&PZ$ZP#A`!KZK!ZA$eP!Ke%6o,!,o",
"6&!Ke'%`P%PZ!61%-!'6'!-!,:,!,:,!,:,#ZejaoeA!,#V&,jF%'$,o;%6j'j;$PZ$eF!PZ$V!Ke&!PZ&!PZ$ZP$1o1$PZ!'jF$'o6&!j;j1e;j!UP`A!V!",
"PZ$6o,!PZ$Fj&1o1&;o$j;#6j!UFU!e6$;o6o6&`P!K`&!,o;$-`P.!PZ`eo`$AZ!;o;!UA!KsF!KsF!KsF#,UP16j1'%V&Z`#6,&Ke&KZ!ZP$P`6;U`'!PZ",
"$V!Ke>'!P`+!P`8`P$1o1$P`6Pj'$'o6&!j;P`KAj!VFZ!V!PZ$U`#PZ#,eU&1o1&,o6!1o'#1o!`6`!o,%`jZ&!;o1j;&!UZ$1soj!Pj1!1o;#6e!F`F!e6",
"&Fj1&Fj1&Fj1$Z;!6Z&!V%;o1#o;&jK&`F!F`$PoekA#PZ$V!Koh6!PsF!PoloP$1o1$PojoU%'o6&!jA;o1Aj!V,o,V!Pj[k;#Pjfo`,&1o1&!eK!Fe$'o,",
"j!j'o'%AoA&#g&!6o1%-`P-'!P`$`P#'o'UKU!o'%6o;&6o;&6o;#',j6,KP,&V%eP$o;%;o,%'o6'6o'#PZ#6jF!PZ$V!Ke&!P`/'!PZ$ZP$1o1$PZ!Ao,$",
"'o6&!jA!,!Aj!V!ZFV!PeKF6%P`;Ko,&!1o1&!PZ!ZP%j6e!e6j&`f&#AoA&!eP&#`P%PZ$ZU$j1e1e,e%,jF&,jF&,jF$Aeofje`&V%j`Ro;%[&;s;#PZ$K",
"e!PZ$V!Ke&!P`&!PZ$ZP$1o1$PZ#eP$'o6&!jA$Aj!V!A`PU!PZ&#PZ#eK&!1o1&!;o'j;%`KZ!ZF`%Aj,jF&!1o1&Fj'&#`P%PZ$ZU$Z;e!j;Z$'eU&'eU&",
"'eU&6Z#e1&!V%To;$,o;&PZ)ZP#PZ$PZ!PZ$jF!Ke&!P`&!PZ$ZP$1o1$PZ#Ao,#'o6&!jA$Aj!V!'oZU!PZ&#PZ#Fj'&1o1&!'oAo'%UZP!PZU$'jF!Fj'&",
"1o1%'jA&$`P%PZ$ZU$PUZ!`PK$Z`'&Z`'&Z`'&FP!'o'$,2[2&#o;$Kj&!eK$Fe#P`16Kj;!P`1;Uj,!Ke5!P`&!PZ$ZP!'1AoA1'!PZ$eU#'oF4'!jA$Aj!",
"V#UoU!PZ&#PZ#'jF&1o1&#b&PjF!KjK$Ke#'jK&1o1%Pe64%`P%PZ$ZU$AjK!PeA#Fo;3,!Fo;3,!Fo;3,#UA!1e%Uso'&!o;$jK&,o6$6o,!PqZA#PpjZ1#",
"Ks`!P`&!PZ$ZP!AsA!PZ$Ao,!'so6!jA$Aj!V#;oU!PZ&#PZ$Kj&1o1&#KoK&Fo;!AoA#,oA$Ao,%1o1%ZsZ%`P%PZ$ZU$6oA!Fo1#UsU!UsU!UsUxxwvut&",
"%KZ&#KlP&$K6%6j1$,j6&'&%'&!Uj,$UK#Uj,$UK!1[j$UK$FnK#PkeU1#PU&$PkeU1$PZ$ZP$PU&!'j6!Q$KlP!PK#'j6&!`l;&#'j6&!KlP&,nk,%K6&A`",
"kPt#';'$,;'&#K`&#,7`U&$jA&`U$P`%K]K&K]K$1A`,#;e'!1A`,#;e'#'U6#;e'$UZPe7,#P`6;Zj,!UZ&$P`6;Zj,#PZ$ZP#UejV1$'o6!ZF$,7`U!ZF#",
"'o6&!8o;&#'o6&!,7`U&'7AoA7'%jA%,oF7,!`6&%PoP#ZoeoZ&!K`$'6'%ZU&'7jP71#6o,!,o6$UA$AU$UA$AU%`,!,j6%`,!,j6$'UZ!,j6%eG`&PZ#'j",
"F!UjVP;%PZ#'jF!espe!F`eG,$'o6!P6&!ZU!P6#'o6&%o;&#'o6!,&#ZU!'&!1o1%'7jP71!Fe&p`6&#;Z;!;`'!,o6&K`$`Z&ZU&6so`$`P!K`$'Z!U`Z'",
"Z'!'Z,jZU!Z'$`,!ZK&`,!ZK$7!`,ZK%'o6F`&PZ$V!UeLUoP$PZ$V!,U`.`U,#PZ'A6$'o6&%ZU&'o6&%o;&#'o;F`&#ZU;j'&1o1%6so`!K`&rZ6t%ZP&K",
"`#Fj,&ZU&$jA&!;o1j;$6K1Z!F1K6!6K,Z!U1K6!AZeb%AZeb%,[KA`&6o'Fe7'!,U`,'!V!UZ$Fo'!,U`,'!V#P`8`P$PZ`Zje'!'o6&%ZU&'o6#2&o;#,6",
"$'ojZ,&#Zj`1&!1o1&#jA%K`&soZ6&#'UjqP&K`!1oA&!ZU&$jA&2jej2#;K6U$K;!;K,jPU!K;%,j1&#,j1&#,j1&Fe!Fok,!`q;!V!UZ$6o,!`q;!V#Pol",
"oP$Po,!'o;!'o6&%ZU&'o6!,p'%o;#eo6#FoK&$;jZ&!1KPoPK1&jA%K`&sqP&!U`1(ZP&Koej`&#ZU&$jA%'lol'!;K6Z!6,K;!;K,`AU!K;%`F!Z1&`FKZ",
"U&`F!Z1$UZ1K`&PZ$V!UZ#'UZ$PZ$V#PZ$ZP$P`$ZP!'o6&%ZU&'o6!'e`&o;#Ue,!P`o6&#Ae`U&!1PUoUP1$KZo`['!K`&sojF'&!jF$eP&Ke;Pj,&!ZU&",
"$jA&#1o1%,U!ePe'U,!,U,Z!e'U,$KZ!L,%KZ'P!P;$KZ!L,$eojo`&PZ$V!Uoejo`6$PZ$V#PZ$ZP$PZ$ZU!'o6&%ZU&'o6&%o;&!1'o6&#1'ZU&$1o1&;A",
"jUB'!K`&rjF'%FoF!P`1,FjP&K`#`Z&!V&$jA%'lol'#`1!6'1`$`61!,6`$1j,1`!`'#1j,#,e1#1j,1`!`'!'o;!F`&PZ$jF!U`7,&PZ$jF#PZ$ZP$PZ$Z",
"U!'o6&%V&'o6&%oA&#'o6&%V&$1o1&#jA%K`&pjF'&!KoK!,ZojKZP&K`#1oA&Aj;2'%UZ62#2;o;2$,ZPFPZ,$,ZPFPZ,#'eA!AeZj'!'eA#AZ1#'eA!AeZ",
"j'!6o,!Fe2,#P`1;Uj,!UZ&$P`1;Uj,#PZ$ZP$PZ$ZU!'oF4'%Aj;2'!'oF4'%UZ62%'oF4'%Aj;2'%1o1&#UZ62!K`&jF'&%'%'&%K`$Kj,%'Pjp6%1ZpZ%",
"1o1&#,6,&#,6,%PU&`'!PU#6j[A!PU&`'!Fj#FqK#PpjZ1#UZ&$PpjZ1$PZ$ZP$PZ$ZU!'so6%'Pjp6!'so6%1Upj%'so6%'Pjp6%1o1&#1ZpZ!K`&'xwv%'",
";,&';1&'6'&1;,$,1$1,$61!,6&!'&#';,1`,$61!16&!',&%,'&`if`&';'&%;K!K;&#6';',6$,1!61$'6!16$'6'$'6'!-!'6'!-#6,$,6#,:,#6&1&'K",
"'%,8,%,1$'6'&$,t!PjeoZ'#Pj`oU$PlP#'ej`j`'!UZ$ZU#`ejFjf%KoK&UoepK$ekUke%,jK&%Kj,%FspF&,o,&#;`jA!Fj`;%,oUe[eU#PZ`eo`#;oKej",
"e1#jK$Fj#AZ!;o;!UA#Kj'!'jP#KsF!,oP;K;Ko,%,o,%AqK%Fe$Z`&$1j1&%1o6!'jA!1j,!,jA!1o1!1o1!Ae#'Z;!UZ$ZU!,`'1o;!j;$FoF%1o1!Po;#",
";j!;o;!j;#'`Zt!Z`'$'ese'$'!,o,!'$;`jF'$'Kj`;#,o6,o,6o'!Pj1!1o;!;oF!'`P#P`$`P#6e!F`F!e6$eU!Pe'&!Fj1#;oZF`oA&,o,&#`K%Fe#Ao",
"1&$`ZtP`$6;!PU$V!PZ$ZP!Fj1&UZ$ZU%'o,!`FtPU!6jZP#F`!,o,!`F#Ue't!'eU%AsA$6jU6o6Uj6!UeF'&#'KjU!,o,'o',o'!P`$`P!;o,#Fe#6o,!,",
"o6#'o'UKU!o'$1oKo1&!6o;$;`$e;$QUoUQ&`K%Fe!,jF&$Fj,tV&!ZjbjZ!ZP$PZ!,`ojZ;#UZ$ZU#;ZeofoF!6O6#UP'eAPU#F`!,ofoF!AoUOA!AOUoA%",
"eqe&;UjojU;#UeA&%;`U!,o,'o',o'!PZ$ZU!;o$,6$jF!Fj%j1e1e,e&KoK&!,jF%FP$UK$QUoUQ&`K%Foeje%'U'!1o;t!V&!ZU4,!ZP$PZ$1;Uo;!UZ$Z",
"U!,o;1o6-'!6T6#UPV!PU#F`!,o;7,!FoYUF!FYUoF%AqA&#ZeU&AfA&AfA#,o,'o',o'!PZ$ZU!;o&$P`!`P%Z;e!j;Z&`j`&'eU&6e'!'j;&,o,&#`K%Fj",
"AKo1$,f,`Ut#Fe$G!KZ$(!F`$`F!2$`P!Fe$eF!F`!'o,!'t!Kje'!`F#F`!,o,&U`'t!'`U&!eoe&#Kj,jK&!Af;!;fA%,o,'o',o'!PZ$ZU!;o&$6o1o6%",
"PUZ!`PK%Kj,jK%Z`'&FoeUjoK&,o,&#`K%Fe#Z`%,ek't#,oF,;j;!,oA'6j;!,oF1Fo,!Aj6'6jA!,oF,Fo'!6j,AoF,j6$FoF%;oP,Ao,#1o1FoF1e1#'e",
"Ut!Ue'&!AoA&!,o;!;o,&#AP!PA&!,o,'o',o'!PZ$ZU!;o&%eZe&AjK!PeA$1o;!;o1#Fo;3,!,jA,A1Aj,#(1o1(#3eU2'!Fe#,jF%,e;t%AZoeK$A`oeK",
"$AZoZ;#'UjojP$A`o`A$Poe1eoU%KoK%`Zeo`A%UojAjoP%1jA&%Aj1&$Z&$1$1t&%,o,'o',o'!PZ$ZU!;o&%KoK&6oA!Fo1#'eU$Ue'!UsU#'&'$jsj#Zs",
"o;!Fe$Ao1%'t&#'&#'&#'&#,&#'&!'$'&!'%,F#'&#'$'&!,1&%1,xxwu!'F'&!'ZF!;`,#1F6#6H;#,F1$,`;!FZ'%1,!6@6$;j;&'t&,K$P,#'&#1'!'1#",
"`P!P`!6j6!,1$'jF!'eF#Ke!;j1$1%1#,UZ1!6Z!FnF#6$A'!F!'A#1`1$F6!6F$1`1!FMF!'TQ'!FMF!,3,!FOF!ONt%,o,&!Z`'1jA#;eKeA!;LeP!Ae`F",
"%Aj1'`Z$,oF!PiP$`U`%UKU'!,Z`Z,#QK#'`U'['#U`!Aj,#Pe!'jF!`U!U`!1o1!KU,#Fj'#P`!'jF![$`o`#KoK!Z`Kj,Ke!,9eK!PoP!'oA6o6Ao'#Ke'",
"!'oU!Uo'!'eK#PWP!'OL'!PWP!ele!KYK!YXt%,o,&Kj,'eK$F6!;Z$PK#;'PF&Pe',jK#;o1t6j'e6$1;U6!KF!FK!,P!P,#'`jZ'$;o(oA#;o'![!`P!P`",
"!1o1!'Fj'!ZU$`U!;o,'oA#1q1!FoF!j;!PpA&!`K!FoF!'o;1o1;o'$;,#,'!',#,;vuLPoPL#6o1![&#UF#,jZ1$PF&!`Z!6o6!Kjt!UF!FZ#1`;P6!PA!",
"AP!1K!K1$ZoU%Kj!1o1#Pe#jK!ZP!P`!1o1!ZeU#ZU#1o;!;o,'oA$eoe$'#,'#,'&#1,&!'#'#'vut&#VZoZV#'eK!Aj,%'ZP&FU$PF&1jA!Pe'!UZ#K_K!",
",j'!'j1!,ZAU6!1`U`,#ZAZ$[,`U#'jK!Ue#'jF!Ao,!A;!;A!'K'!'%1P'!;P$P;!AK%1wut%,o,&1oA!Pe'#,`F$U6!;Z$PF%'eP!Ao1#;6#;J;!L$L#',",
"($,%'6$,U'!'Z,!,K,!K6#,A'!6;wvut,o,&!Ao1!`Z#Uj[P!6eZe;!KZfZ,#Z`!1oAxvt$'P'&#;6!,A,!181#'6'#19'!,A,!6;xvt!",
0 };
/* LG_FONT_END */
#endif

#if !defined(LOGUIA_NO_DEFAULT_FONT) && !defined(LOGUIA_NO_FONT_2X)
/* LG_FONT2X_BEGIN: made by tools/bake_font.c, do not edit by hand.
   Font: JetBrains Mono (SIL Open Font License 1.1), 30 px. */
#define LG__ATLAS2_W 512
#define LG__ATLAS2_H 259
#define LG__FONT2_LINE 40
#define LG__FONT2_GLYPHS 332
static const lg_glyph lg__glyphs2[LG__FONT2_GLYPHS] = {
{32,0,0,0,0,0,31,18},{33,374,151,6,23,6,9,18},{34,128,244,10,10,4,9,18},{35,226,176,16,22,1,9,18},{36,18,0,14,32,2,4,18},{37,287,125,18,24,0,8,18},
{38,306,125,19,24,0,8,18},{39,139,244,4,10,7,9,18},{40,79,0,10,30,5,5,18},{41,90,0,10,30,3,5,18},{42,467,223,16,16,1,12,18},{43,0,244,16,15,1,14,18},
{44,144,244,6,10,6,26,18},{45,343,244,10,4,4,19,18},{46,298,244,6,6,6,26,18},{47,248,34,14,29,2,6,18},{48,326,125,14,24,2,8,18},{49,243,176,15,22,2,9,18},
{50,381,151,14,23,2,8,18},{51,396,151,14,23,2,9,18},{52,259,176,13,22,2,9,18},{53,411,151,14,23,2,9,18},{54,426,151,16,23,1,9,18},{55,273,176,15,22,2,9,18},
{56,341,125,16,24,1,8,18},{57,443,151,16,23,1,8,18},{58,75,223,6,18,6,14,18},{59,289,176,6,22,6,14,18},{60,275,223,14,17,2,13,18},{61,151,244,14,10,2,16,18},
{62,290,223,14,17,2,13,18},{63,460,151,12,23,3,9,18},{64,263,34,16,29,1,8,18},{65,296,176,16,22,1,9,18},{66,313,176,14,22,2,9,18},{67,358,125,14,24,2,8,18},
{68,328,176,14,22,2,9,18},{69,343,176,13,22,3,9,18},{70,357,176,14,22,2,9,18},{71,373,125,14,24,2,8,18},{72,372,176,14,22,2,9,18},{73,387,176,12,22,3,9,18},
{74,473,151,14,23,1,9,18},{75,400,176,15,22,2,9,18},{76,416,176,14,22,3,9,18},{77,431,176,14,22,2,9,18},{78,446,176,14,22,2,9,18},{79,388,125,14,24,2,8,18},
{80,461,176,15,22,2,9,18},{81,280,34,14,29,2,8,18},{82,477,176,15,22,2,9,18},{83,403,125,14,24,2,8,18},{84,493,176,16,22,1,9,18},{85,488,151,14,23,2,9,18},
{86,0,200,16,22,1,9,18},{87,17,200,18,22,0,9,18},{88,36,200,16,22,1,9,18},{89,53,200,16,22,1,9,18},{90,70,200,14,22,2,9,18},{91,295,34,8,29,6,6,18},
{92,304,34,14,29,2,6,18},{93,319,34,8,29,4,6,18},{94,87,244,14,12,2,9,18},{95,399,244,16,3,1,31,18},{96,312,244,8,5,4,7,18},{97,82,223,14,18,2,14,18},
{98,0,176,14,23,2,9,18},{99,97,223,14,18,2,14,18},{100,15,176,14,23,2,9,18},{101,112,223,14,18,2,14,18},{102,85,200,15,22,1,9,18},{103,30,176,14,23,2,14,18},
{104,101,200,14,22,2,9,18},{105,418,125,15,24,2,7,18},{106,101,0,12,30,2,7,18},{107,116,200,15,22,2,9,18},{108,132,200,17,22,0,9,18},{109,305,223,16,17,1,14,18},
{110,322,223,14,17,2,14,18},{111,127,223,14,18,2,14,18},{112,45,176,14,23,2,14,18},{113,60,176,14,23,2,14,18},{114,337,223,13,17,3,14,18},{115,142,223,14,18,2,14,18},
{116,150,200,15,22,1,9,18},{117,157,223,14,18,2,14,18},{118,351,223,16,17,1,14,18},{119,368,223,18,17,0,14,18},{120,387,223,16,17,1,14,18},{121,75,176,16,23,1,14,18},
{122,404,223,14,17,2,14,18},{123,328,34,14,29,2,6,18},{124,343,34,4,29,7,6,18},{125,348,34,14,29,2,6,18},{126,253,244,14,7,2,17,18},{160,0,0,0,0,0,31,18},
{161,92,176,6,23,6,14,18},{162,197,95,14,26,2,10,18},{163,99,176,15,23,2,8,18},{164,419,223,16,17,1,11,18},{165,166,200,16,22,1,9,18},{166,363,34,4,29,7,6,18},
{167,62,95,14,28,2,8,18},{168,321,244,12,5,3,7,18},{169,495,200,16,20,1,8,18},{170,166,244,8,10,5,8,18},{171,17,244,17,15,0,15,18},{172,268,244,12,7,3,19,18},
{173,354,244,10,4,4,19,18},{174,0,223,16,20,1,8,18},{175,416,244,10,3,4,8,18},{176,175,244,10,10,4,8,18},{177,172,223,16,18,1,13,18},{178,35,244,10,15,4,5,18},
{179,64,244,10,14,4,6,18},{180,334,244,8,5,6,7,18},{181,115,176,14,23,2,14,18},{182,77,95,15,28,1,9,18},{183,305,244,6,6,6,18,18},{184,236,244,7,8,6,30,18},
{185,75,244,11,14,4,6,18},{186,186,244,8,10,5,8,18},{187,46,244,17,15,1,15,18},{188,183,200,18,22,0,9,18},{189,202,200,18,22,0,9,18},{190,221,200,18,22,0,9,18},
{191,130,176,12,23,3,14,18},{192,368,34,16,29,1,2,18},{193,385,34,16,29,1,2,18},{194,402,34,16,29,1,2,18},{195,419,34,16,29,1,2,18},{196,436,34,16,29,1,2,18},
{197,114,0,16,30,1,1,18},{198,240,200,18,22,0,9,18},{199,131,0,14,30,2,8,18},{200,453,34,13,29,3,2,18},{201,467,34,13,29,3,2,18},{202,481,34,13,29,3,2,18},
{203,495,34,13,29,3,2,18},{204,0,65,12,29,3,2,18},{205,13,65,12,29,3,2,18},{206,26,65,12,29,3,2,18},{207,39,65,12,29,3,2,18},{208,259,200,16,22,0,9,18},
{209,52,65,14,29,2,2,18},{210,146,0,14,30,2,2,18},{211,161,0,14,30,2,2,18},{212,176,0,14,30,2,2,18},{213,191,0,14,30,2,2,18},{214,206,0,14,30,2,2,18},
{215,102,244,12,12,3,15,18},{216,212,95,16,25,1,8,18},{217,221,0,14,30,2,2,18},{218,236,0,14,30,2,2,18},{219,251,0,14,30,2,2,18},{220,266,0,14,30,2,2,18},
{221,67,65,16,29,1,2,18},{222,276,200,15,22,2,9,18},{223,143,176,15,23,2,8,18},{224,229,95,14,25,2,7,18},{225,244,95,14,25,2,7,18},{226,259,95,14,25,2,7,18},
{227,274,95,14,25,2,7,18},{228,289,95,14,25,2,7,18},{229,167,95,14,27,2,5,18},{230,189,223,17,18,0,14,18},{231,434,125,14,24,2,14,18},{232,304,95,14,25,2,7,18},
{233,319,95,14,25,2,7,18},{234,334,95,14,25,2,7,18},{235,349,95,14,25,2,7,18},{236,449,125,15,24,2,7,18},{237,465,125,15,24,2,7,18},{238,481,125,15,24,2,7,18},
{239,0,151,15,24,2,7,18},{240,16,151,14,24,2,8,18},{241,31,151,14,24,2,7,18},{242,364,95,14,25,2,7,18},{243,379,95,14,25,2,7,18},{244,394,95,14,25,2,7,18},
{245,409,95,14,25,2,7,18},{246,424,95,14,25,2,7,18},{247,207,223,12,18,3,12,18},{248,37,223,16,19,1,13,18},{249,439,95,14,25,2,7,18},{250,454,95,14,25,2,7,18},
{251,469,95,14,25,2,7,18},{252,484,95,14,25,2,7,18},{253,281,0,16,30,1,7,18},{254,93,95,14,28,2,9,18},{255,298,0,16,30,1,7,18},{256,108,95,16,28,1,3,18},
{257,46,151,14,24,2,8,18},{258,84,65,16,29,1,2,18},{259,0,125,14,25,2,7,18},{260,101,65,17,29,1,9,18},{261,61,151,15,24,2,14,18},{262,315,0,14,30,2,2,18},
{263,15,125,14,25,2,7,18},{264,330,0,14,30,2,2,18},{265,30,125,14,25,2,7,18},{266,345,0,14,30,2,2,18},{267,45,125,14,25,2,7,18},{268,360,0,14,30,2,2,18},
{269,60,125,14,25,2,7,18},{270,119,65,14,29,2,2,18},{271,159,176,18,23,1,9,18},{272,292,200,16,22,0,9,18},{273,178,176,16,23,2,9,18},{274,125,95,13,28,3,3,18},
{275,77,151,14,24,2,8,18},{276,134,65,13,29,3,2,18},{277,75,125,14,25,2,7,18},{278,148,65,13,29,3,2,18},{279,90,125,14,25,2,7,18},{280,162,65,13,29,3,9,18},
{281,92,151,14,24,2,14,18},{282,176,65,13,29,3,2,18},{283,105,125,14,25,2,7,18},{284,375,0,14,30,2,2,18},{285,390,0,14,30,2,7,18},{286,405,0,14,30,2,2,18},
{287,420,0,14,30,2,7,18},{288,435,0,14,30,2,2,18},{289,450,0,14,30,2,7,18},{290,33,0,14,31,2,8,18},{291,3,0,14,33,2,4,18},{292,190,65,14,29,2,2,18},
{293,205,65,18,29,-2,2,18},{294,309,200,18,22,0,9,18},{295,328,200,16,22,0,9,18},{296,224,65,12,29,3,2,18},{297,107,151,15,24,2,7,18},{298,139,95,12,28,3,3,18},
{299,195,176,15,23,2,8,18},{300,237,65,12,29,3,2,18},{301,123,151,15,24,2,7,18},{302,250,65,12,29,3,9,18},{303,48,0,15,31,2,7,18},{304,263,65,12,29,3,2,18},
{305,436,223,15,17,2,14,18},{308,465,0,18,30,1,2,18},{309,484,0,15,30,2,7,18},{310,0,34,15,30,2,9,18},{311,16,34,15,30,2,9,18},{312,452,223,14,17,3,14,18},
{313,276,65,14,29,3,2,18},{314,291,65,17,29,0,2,18},{315,32,34,14,30,3,9,18},{316,47,34,17,30,0,9,18},{317,345,200,14,22,3,9,18},{318,360,200,17,22,0,9,18},
{319,378,200,14,22,3,9,18},{320,393,200,19,22,0,9,18},{321,413,200,17,22,0,9,18},{322,431,200,17,22,0,9,18},{323,309,65,14,29,2,2,18},{324,139,151,14,24,2,7,18},
{325,65,34,14,30,2,9,18},{326,120,125,14,25,2,14,18},{327,324,65,14,29,2,2,18},{328,154,151,14,24,2,7,18},{329,169,151,17,24,-1,7,18},{330,152,95,14,28,2,9,18},
{331,211,176,14,23,2,14,18},{332,339,65,14,29,2,3,18},{333,187,151,14,24,2,8,18},{334,80,34,14,30,2,2,18},{335,135,125,14,25,2,7,18},{336,95,34,14,30,2,2,18},
{337,150,125,14,25,2,7,18},{338,202,151,17,24,1,8,18},{339,220,223,16,18,1,14,18},{340,354,65,15,29,2,2,18},{341,220,151,13,24,3,7,18},{342,110,34,15,30,2,9,18},
{343,165,125,15,25,1,14,18},{344,370,65,15,29,2,2,18},{345,234,151,13,24,3,7,18},{346,126,34,14,30,2,2,18},{347,181,125,14,25,2,7,18},{348,141,34,14,30,2,2,18},
{349,196,125,14,25,2,7,18},{350,156,34,14,30,2,8,18},{351,248,151,14,24,2,14,18},{352,171,34,14,30,2,2,18},{353,211,125,14,25,2,7,18},{354,386,65,16,29,1,9,18},
{355,403,65,15,29,1,9,18},{356,419,65,16,29,1,2,18},{357,226,125,15,25,1,6,18},{358,449,200,16,22,1,9,18},{359,466,200,15,22,1,9,18},{360,186,34,14,30,2,2,18},
{361,242,125,14,25,2,7,18},{362,436,65,14,29,2,3,18},{363,263,151,14,24,2,8,18},{364,201,34,14,30,2,2,18},{365,257,125,14,25,2,7,18},{366,64,0,14,31,2,1,18},
{367,182,95,14,27,2,5,18},{368,216,34,14,30,2,2,18},{369,272,125,14,25,2,7,18},{370,451,65,14,29,2,9,18},{371,278,151,14,24,2,14,18},{372,466,65,18,29,0,2,18},
{373,293,151,18,24,0,7,18},{374,485,65,16,29,1,2,18},{375,231,34,16,30,1,7,18},{376,0,95,16,29,1,2,18},{377,17,95,14,29,2,2,18},{378,312,151,14,24,2,7,18},
{379,32,95,14,29,2,2,18},{380,327,151,14,24,2,7,18},{381,47,95,14,29,2,2,18},{382,342,151,14,24,2,7,18},{383,482,200,12,22,4,9,18},{8211,365,244,14,4,2,19,18},
{8212,380,244,18,4,0,19,18},{8216,195,244,6,10,6,9,18},{8217,202,244,6,10,6,9,18},{8220,209,244,12,10,3,9,18},{8221,115,244,12,11,3,8,18},{8222,222,244,13,10,3,26,18},
{8226,244,244,8,8,5,16,18},{8230,281,244,16,7,1,25,18},{8364,357,151,16,24,0,8,18},{8592,237,223,18,18,0,12,18},{8594,256,223,18,18,0,12,18},{9654,17,223,19,20,0,10,18},
{9660,54,223,20,19,-1,11,18},{10003,484,223,16,16,1,12,18},
};
static const char *const lg__font2_rle[] = {
"p&%B,t(t$-t$'1t$6V6t&'!'t&$'1&%6V6t$',&%,LAt&#AKF'&$6L6t2%1,&!6'%'6&!,LAt&#AKF'&$6L6&%6'%'6t$AL'&$1'%'1t$6L,&$1L;t#,1&$'",
"FKF%;L,&#6L6t!;L1&$AK1%1KA&FK1%6K;&%-t$-t&$FKFt#'LFt&#p&$Ap'tZo'&%1Zjpj`;&%1joj,tZokoZt!'A`K!K`A't%6joj'&!ZokoZ&%,Zjq`A&",
"#Pp;t&;pK&$6qj6&#'`pe1!1o`&`oj,#1p`&!Pp;t&;pK&$6qj6&#`oj,#1p`t!;pK&$`oj,#1joZt!1pZ&$1jqAtFpZ&$;pZ'!FpP&#6qj6&%Aqj6&#PoZ%",
"ZoP&UoU%eoK&$PpPt!UpKt&!ZqZt'`qUu&'eoZt!Zo'&$KsqU&$PqF&%,oU#Uo,&%FjpK!KpjFt$PqA&,oU#Uo,&#AsqZ'&!`oj1t$,joZ&$6paoj6&!FoeP",
"ojA`oK%,qF#Fq,&!`oj1t$,joZ&$6paoj6&,qF#Fq,&%,joZ&$,qF#Fq,&%'eoe'&#1joeUpA&%eq1&$;pZKpP&#6paoj6&#ApU`oj6&!6pF(Fp6&;ojF'1P",
"p1&#'r'&%,qjt&ZpPpZ&$'`ojKpUu%FpAt!Zo'&#6q`FAZqF&#;q1&%Fo6#6oF&$Pqj;!;jqPt#Aq1&Fo6#6oF&!1qeK;ZqP&!'eoe't#`oe'&#6jo`(Zoj6",
"&Zo6!;jpj,&jp6#6pe&#'eoe't#`oe'&#6jo`(Zoj6&jp6#6pe&%'`oe'&%jp6#6petUoj1&#1joe,!UpA&$Ppet;rP&#6jo`(Zoj6&ApU!'`oj6&!KsoK&#",
"KsoA&%ZpZt!epUt%Zp;!;pZ&!'`oj6!;pUu#'jp't!`o,&#ep6%,pj'&#1F1t6oK#Ko6&!'ZpjF'$'FjpUt#6F1&!6oK#Ko6&!ZpF%'jp1&!1UZAt!;[1&#'",
"UZP'#'PZU'%;A'#,B'&!'A1%1A'&$1UZAt!;[1&#'UZP'#'PZU'%'A1%1A'&%;V1t,A1%1A,&%1[;&#'PZU,$KZU1&$;A't!;[K&#'UZP'#'PZU'$,VK$'PU",
"P'&!1PaP1&%1P[K,t'<'t!'A;'t$A[;$6[A&FV1$;V;u!PpU&%6Ujq`F%'pe&!Zp6u!eo[oe&#Fpe'&#'epAu!eo[oe&#joj'&PpAxxvt%1q;&$KsqZ'#6pK",
"&!ApFu!,FfF,&!'jp6&%6pj'u,FfF,&!,pU&!6pK&#-t$-t$-t$-t$-wu&#',t$',t$',t$',t$-u&$-u&$-vut&!AW'&#6qP`oKjpP#;p;&#-'#AOL'u#Fp",
"Kt!KpF#1OOt&%6pF&#-'%1ZjpjZ1&#1ZjpjZ1&#1ZjpjZ1&#1ZjpjZ1&#1ZjpjZ1%6f;&!;f6!6f;&!;f6!6f;&!;f6!6f;&!;f6!,L;&$6L,!,L;&$6L,%,",
"Zjq`A&#,Zjq`A&#,Zjq`A&#,Zjq`A&#1Zjpj`;&#,KVA,!1L,%1Zjpj`;&#,KVA,!1L,%1Zjpj`;&#,KVA,!1L,t#ZeZ&1OOut&!ep1!Zo(ep,!;p;t#esp'",
"&!6f;&!;f6#eojt#'pe#Fss'&#`g&#6pFt$KsqK&KsqK&KsqK&KsqK&KsqK$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!'joe&$`oj'!'joe&$`oj'$As",
"qZ'%AsqZ'%AsqZ'%AsqZ'%KsqU&Aso;Fp1$KsqU&Aso;Fp1$KsqU&Aso;Fp1t#eoe&Fss'ut%'pZ#Zo'!Kp;!;p;t#`mq'&!6pA&!Ap6!'p`t$`p'!Ankq'&",
"!,r1&!6pFt#6q`B`q6$6q`B`q6$6q`B`q6$6q`B`q6$6q`B`q6#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6#Pp6&!1pP$Pp6&!1pP$1qeK;ZqP$1qeK;Z",
"qP$1qeK;ZqP$1qeK;ZqP$6q`FAZqF$6r[ojKp1#6q`FAZqF$6r[ojKp1#6q`FAZqF$6r[ojKp1t#eoe&Ankq'u#,KVA,!1L,!1pK#Zo'!,V;!;p;t&#Pp'&!",
"6pA&!Ap6!1pPt$Pp1&%Up'&!AokoF&!6pFt#ep6%6pe$ep6%6pe$ep6%6pe$ep6%6pe$ep6%6pe#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6#1pU&!Kp6",
"$1pU&!Kp6$ZpF%'jp1#ZpF%'jp1#ZpF%'jp1#ZpF%'jp1#ep6%,pj'#`pK'#,Ujp1#ep6%,pj'#`pK'#,Ujp1#ep6%,pj'#`pK'#,Ujp1t#eoet$Up'u!Aso",
";Fp1#jo`#Zo'&!;p;#6JF6&#Pp'&!6pA&!Ap6!;pAt$Fp;&%Up'&!Uo[oZ&!6pFt!'pe&!ep'!'pe&!ep'!'pe&!ep'!'pe&!ep'!'pe&!ep'!6pA&!Ap6!6",
"pA&!Ap6!6pA&!Ap6!6pA&!Ap6$`p'&eoe&`p'&eoe%joj'&PpA#joj'&PpA#joj'&PpA#joj'&PpA!'pe&!Zp6!'pj&,q1!'pe&!Zp6!'pj&,q1!'pe&!Zp6",
"!'pj&,q1t#eoet$Up'u6r[ojKp1#Up,!Zo'&!;p;#PsoK&#Pp'&!6pA&!Ap6!Ap6t$6pA&%Up'&!joLoj&!6pFt!1pK&!Kp1!1pK&!Kp1!1pK&!Kp1!1pK&!",
"Kp1!1pK&!Kp1!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6$ApF%6pK&ApF%6pK$,pU&!6pK!,pU&!6pK!,pU&!6pK!,pU&!6pK!6pK&!ApF!6pP&!`p1!6",
"pK&!ApF!6pP&!`p1!6pK&!ApF!6pP&!`p1t#eoet$Up'u`pK'#,Ujp1#;pjAZo'&!;p;#PsoK&#Pp'&!6pA&!Ap6!Fp6t$6pF&%Up'&1p<p1&6pFt!;p;&!;",
"p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6$'jo`%Pp,&'jo`%Pp,$6pF&#-'!6pF&#-'!6pF&#-'!6pF",
"&#-'!;p;&#-'!;p;&!Kp1!;p;&#-'!;p;&!Kp1!;p;&#-'!;p;&!Kp1t#eoet$Up't&%'pj&,q1#'`s1&!;p;&!,pK&#Pp'&!6pA&!Ap6!Fp6t$6pF&%Up'&",
"Fp(pF&6pFt!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6%Pp1$joZ&#Pp1$joZ%6pFt!6pFt!6",
"pFt!6pFt!;p;t!;p;&!Fp1!;p;t!;p;&!Fp1!;p;t!;p;&!Fp1t#eoet$Up't&%6pP&!`p1%FjrZ,%;p;&!,pK&#Pp'&!6pA&!Ap6!Fp6t$6pF&%Up'&Zo`#",
"`oZ&6pFt!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6%1pP#;pA&#1pP#;pA%6pFt!6pFt!6pF",
"t!6pFt!;p;t!;p;&!Fp1!;p;t!;p;&!Fp1!;p;t!;p;&!Fp1t#eoet$Up't&%;p;&!Kp1&,Zrj;$;p;&!,pK&#Pp'&!6pA&!Ap6!Fp6t$6pF&%Up'&joP#Pp",
"'%6pFt!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&`oj'!Uoj'&$`oj'!Uoj'%6pFt!6pFt!6",
"pFt!6pFt!;p;t!;p;&!Fp1!;p;t!;p;&!Fp1!;p;t!;p;&!Fp1t#eoet$Up't&%;p;&!Fp1&#Zo`qK#6pK&!;pF&#Pp'&!6pA&!Ap6!Fp6t$6pF&%Up'%6p;",
"#;p6%1pP&!1pP!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&Ap;'pP&%Ap;'pP&6pFt!6pFt!",
"6pFt!6pFt!;p;#6JF6!6pK&!Zp1!;p;#6JF6!6pK&!Zp1!;p;#6JF6!6pK&!Zp1t#eoet$Up't&%;p;&!Fp1&#Zo'Fpj#,pe&!Up6&#Pp'&!6pA&!Ap6!Ap;",
"t$;p;&%Up'%Kp'#'pK%'pj&!KpA!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&'joZ;p6&%'j",
"oZ;p6&6pFt!6pFt!6pFt!6pFt!;p;#PsoK!,pe&'q1!;p;#PsoK!,pe&'q1!;p;#PsoK!,pe&'q1t#eoet$Up't&%;p;&!Fp1&#Zo'!Fp6#jp1%'jp,&#Pp'",
"&!6pA&!Ap6!1pKt$Kp1&%Up'%`oe%eo`&`p;&ep1!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap",
"6&!PpZoet!PpZoe&!6pFt!6pFt!6pFt!6pFt!;p;#PsoK#epA$'Kq1!;p;#PsoK#epA$'Kq1!;p;#PsoK#epA$'Kq1t#eoet$Up't&%6pK&!Zp1&#Zo'!,pP",
"#;qU;6PjpK&$Pp'&!6pA&!Ap6!'pZt$Zp'&%Up'$'pP%Pp'%6qZA6PjpU#;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6!6pA&!Ap6",
"!6pA&!Ap6!6pA&!Ap6&!1rKt!1rK&!6pFt!6pFt!6pFt!6pFt!;p;&!,pK#;qjKPkKp1!;p;&!,pK#;qjKPkKp1!;p;&!,pK#;qjKPkKp1t#eoet$Up't&%,",
"pe&'q1!KV$Zo'#jo`$Psq`$1J`pKI6!6pA&!Ap6#joet$jojtUp'$6pA*Ap6&Fsqe'#;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6",
"!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&#`q,t#`q,&!6pFt!6pFt!6pFt!6pFt!;p;&!,pK$KsoAFp1!;p;&!,pK$KsoAFp1!;p;&!,pK$KsoAFp1t#eoet$Up'u",
"epA$'Kq1!Pp6#Zo'!6pP%;erjA%FssqU!6pA&!Ap6#Up6t!6pUtUp'$KssK&!;ZrjF%;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!;p;&!;p;!6pA&!Ap6",
"!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&#ApZt$ApZ&#6pFt!6pFt!6pFt!6pFt!;p;&!,pK%1P[F1!Fp1!;p;&!,pK%1P[F1!Fp1!;p;&!,pK%1P[F1!Fp1!17&$",
"eoet$Up'u;qjKPkKp1!ApZ'!Zo'!UpA&!'16'&!FssqU!6pA&!Ap6#1pe'&%'ep,t`oj%`ss`&$Zo6'&!6pK&!Kp6!6pK&!Kp6!6pK&!Kp6!6pK&!Kp6!6pK",
"&!Kp6!1pK&!Kp1!1pK&!Kp1!1pK&!Kp1!1pK&!Kp1&#ApAt$ApA&#,pP&!1pP!,pP&!1pP!,pP&!1pP!,pP&!1pP!6pK&!;pFt!Fp1!6pK&!;pFt!Fp1!6pK",
"&!;pFt!Fp1!Kp;&#joZt$`oju#KsoAFp1#`pjF`oAep`u#;oe,&#1pK&!Kp1$UpU&%UpUt1p`$'pU51Up'&!'p`K'&'pe&!ep'!'pe&!ep'!'pe&!ep'!'pe",
"&!ep'!'pe&!ep'!'pe&!ep'!'pe&!ep'!'pe&!ep'!'pe&!ep'&#`oj't$`oj'&$pj&!KpA#pj&!KpA#pj&!KpA#pj&!KpA!,pe&!Up6t!Up,!,pe&!Up6t!",
"Up,!,pe&!Up6t!Up,!;pU&!1pKt#1p`u$1P[F1!Fp1#1esqe,&$'.&%;oj,&$'pe&!ep'$,jpP'&'Ppj,tKpP$;pA&!;p;&#1Aeoe'&ep1%1pe$ep1%1pe$e",
"p1%1pe$ep1%1pe$ep1%1pe$ep1%1pe$ep1%1pe$ep1%1pe$ep1%1pe&#1pPt$1pP&%`p;&ep1#`p;&ep1#`p;&ep1#`p;&ep1#jp1%'jp,t'pj$jp1%'jp,t",
"'pj$jp1%'jp,t'pj#,pj'&Zp;t#KpPutFp1$'KereP'&%Zp;&%ZoUtep1%1pe&1epjK'!'Kjpe1&),F`pj,$Pp,&!'pP&%;p;&;qU<Uq;$;qU<Uq;$;qU<Uq",
";$;qU<Uq;$;qU<Uq;$;pjA16Uq;$;pjA16Uq;$;pjA16Uq;$;pjA16Uq;&#Kp6t$Kp6&%6qZA6PjpU$6qZA6PjpU$6qZA6PjpU$6qZA6PjpU$;qU;6PjpK&*",
",A`pP$;qU;6PjpK&*,A`pP$;qU;6PjpK&*,A`pP$KqU;6PjpU&#),F`pj,utUp,&!,`o1t!ApPteoet;pjA16Uq;&!,eqK!Kq`,&6sq;%eoe&$eoe&!AKPjo",
"j'&!PsqP&PsqP&PsqP&PsqP&PsqP&UsqU&UsqU&UsqU&UsqU&$joet%joet!Ksqe'%Ksqe'%Ksqe'%Ksqe'%Psq`&1sqe'%Psq`&1sqe'%Psq`&1sqe'%Zsq",
"`,&!6sq;ut'pj&$Zo't,joe'tApj[&$UsqU&$'A`oK!Ko`A'&!6soZ1%,pP&$Pp,&ZpjU6&$;ere;&#;ere;&#;ere;&#;ere;&#;ere;&#AereA&#AereA&",
"#AereA&#AereA&$;pFt$;pFt#6`rjF&#6`rjF&#6`rjF&#6`rjF&#;erjA&!1spU,&!;erjA&!1spU,&!;erjA&!1spU,&!AerjF&$6soZ1u&*,A`pP&$Zo'",
"tZp1t#FZq&%AereAt#16!61&%,D1'&!ApA&$;pA&)t$'2't!'2't!'2't!'2't!'2't!'2't!'2't!'2't!'2't6B't$6B't%'16't!'16't!'16't!'16't",
"!'16'&$'E6't'16'&$'E6't'16'&$'E6't'16't,D1'u&!1sqe'&$Zo't2,t&)t!'2'xxwv%1spU,&%,1xxxt!'E6'xxxxxxvt!1fA&#Pf6!'fKt#'fKt!'i",
"g6&$6gA&6f6$AK1%1KA&$6L,!'FK;!1ifUF1t$AKF'&$6L6t#,'&$'FKA%AKF'&!2%1,&AK1%1KA&$6L,!'FK;&#6L6t&#6fK&$','t$-&#ZieF!Kf6t!Fie",
"Z&%,KZf,!;f;!,fUF,&%;f;%,LAt&%AKF't6L6t#2%1,&$6'%'6&#FKF't&1L1&#,FKA&$1,&6'&!6pF&!1p`#'pPt#,pPt!,sq;&$6q`&;p6$PoZ%ZoP&#,",
"joU#Uoj,!1ssPt!;pK&$6qj6&$6UepeP1&!ApP#PpF&'`pe1!1o`&PoZ%ZoP&#,joU#Uoj,&!;r;t&!Pp1&1UeqeU,&#6`jpj`6%esoK!1pPt!Ksoe&$Ajr,",
"!;p;!,rj6&$;p;&Pp;t&#;pKt6qj6&%'`pe1!1o`&#`oj,#1p`&!;pUt%,jo`'&!,eqK&!KpA$eoj,&6pF&!Up6#'pPt#,pPt!'OZp;&$6r,%;p6$6pF(Fp6",
"&!'`o`'!Fp1#1pZMPeqP&%,joZ&$6paoj6&!;spj6&!ApPUpF&!FoePojA`oK&6pF(Fp6&!'`o`'!Fp1&!;p[p;t%'joe&KrjqjA&PsqP$eojN6#eoj't6Nj",
"oe&#1qePK'!;p;!'KPeq6&#;p;&!`oj1t&,joZt6paoj6&$FoePojA`oK&!,qF#Fq,&!FpAt$Zoj,&!'eojUpK&ep`#1qF&6pF&1pZ$'pPt#,pPt&#Ap;&$6",
"pjoK%;p6%KsoK&#Uoj,!6pA$1pF&6ep;&$`oe'&#6jo`(Zoj6%;qZ;A`q6&!ArF&#Zo6!;jpj,&!KsoK&#Uoj,!6pA&!;pZ(Zp;t$ApF%UpZ6'!'6`p6$Fq`",
"B`qA#eoe&#FpAt&eoe&#PpK&;p;&KpF&#;p;&!'eoe't%`oe'&%6jo`(Zoj6&#Zo6!;jpj,&#jp6#6pe&$Up6t!Kp6&!'eoj1!KpK%PpK$jp1&6pF&Zp6$'p",
"Pt#,pPt&#Ap;&$6pUoe%;p6&1PaP1&#1[1#PZF%1pF&!ApP&#;[1&#'UZP'#'PZU'$Zp1%ApU&#A[A&$;A'#,B'&$1PaP1&#1[1#PZF&!'PUP'$KUP't#Zp,",
"$6pF&#Zoe'!'jp1%1pj'!eoe&#,pZt&eoe&#Up,&;p;&,pU&#;p;&#1UZAt$;[1&%'UZP'#'PZU'&!;A'#,B'&$'A1%1A'&$'UZPt,UZA&#KZU1$A[6%;6%'",
"A1&!6pF%1pZ%'pP&#AL,!,pPt&#Ap;&$6p;p1$;p6ut!1pF&!'p`ut#'p`&!eojw&#,pZ%eo`&$1pA!;pZ&!Zp;!eoe&$Zp,t%eoe&#Zp&!;p;&!pU&#;p;w",
"vu&6pF%Zp1%'pP&!6p`#,pPt&#Ap;&$6p1`oP$;p6&#-t$-&#1pF&#ep&$,'t$,'&#1pF&!;Q'&#,'vut&#Kp;$1p6&%eoK!FpA&!ApF!eoe&$;pKt%eoe&#",
"Pp'&;p;&,pK&#;p;wvu&6pF$6pU&'pP&'joj,#,pPt&#Ap;&$6p6Foj$;p6%1ZjpjZ1&#1ZjpjZ1%1pF&!,pZ&6UepeP1&#6UepeP1%'pPt%6UepeP1%6f;&",
"!;f6!6f;&!;f6!6f;&!;f6!,L;&$6L,&%eoj%;ptUoU!Pp,&!,pP!eoe&%joet%eoe&#Kp1&;p;&;pA&#;p;&#`gt$`gt$`gt$`gt$`g&#iifK!iifK!iifK",
"!iifK%6pF$`p1&'pP&UpF$,pPt&#Ap;&$6p6,p6#;p6$KsqK&KsqK$1pF&!FpA%;spj6&;spj6%Zoj't#;spj6$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!'joe&$",
"`oj'&$6pP%Koe&1AF6!Po`!Pp'&!,pP!eoe&%Pp6t$eoe&#Fp6&;p;&Fp1&#;p;&!,r1t!,r1t!,r1t!,r1t!,r1&!sspP!sspP!sspP!sspP%6pZGKpU&!'",
"pP%;pZ%,pPt&#Ap;&$6p;!ZoU#;p6#6q`B`q6$6q`B`q6#1pF%,Fjp,$;qZ;A`q6$;qZ;A`q6$ApjAt;qZ;A`q6#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6#Pp6&!",
"1pP&%Up1%Po`%FrZPo`!Pp'&!,pP!eoe&%1pUt$eoe&#;p;&;p;&Pp'&#;p;&!AokoFt!AokoFt!AokoFt!AokoFt!AokoF&!peON;!peON;!peON;!peON;",
"%6sq1&!'pP$'joj,%,pPt&#Ap;&$6p;!Aoj'!;p6#ep6%6pe$ep6%6pe#1pe]`rK%Zp1%ApU$Zp1%ApU$'ZqjUA,&!Zp1%ApU#6pA&!Ap6!6pA&!Ap6!6pA&",
"!Ap6#1pU&!Kp6&$'jo`&UoZ$1pjLko`!Pp'&!,pP!eoet`oj't#eoe&#Ap6&;p;&Pp'&#;p;&!Uo[oZt!Uo[oZt!Uo[oZt!Uo[oZt!Uo[oZ&!pUt!pUt!pUt",
"!pUt%6pjfjpF&!'pP$UpA&,pPt&#Ap;&$6p;!'pA!;p6!'pe&!ep'!'pe&!ep'!1sre6%'p`&!eoj#'p`&!eoj&AjsU,$'p`&!eoj#6pA&!Ap6!6pA&!Ap6!",
"6pA&!Ap6$`p'&eoe&%ApF&UoZ$Pp,#,p`!Pp'&!,pP!eoetFpAt#eoe&#Poj&!;p;&Ap6&#1L1&!joLojt!joLojt!joLojt!joLojt!joLoj&!pUt!pUt!p",
"Ut!pUt%6pF#,pj'&'peVZpZ&!,pPt&#Ap;&$6p;#UoZ!;p6!1pK&!Kp1!1pK&!Kp1!1peWjp,&1pF&!;Q'!1pF&!;Q'&'K`re,#1pF&!;Q'!6pA&!Ap6!6pA",
"&!Ap6!6pA&!Ap6$ApF%6pK&%`p'&UoZ$Zo`%`o`!Pp'&!,pP!eoet'p`t#eoe&'6eoU&!;p;&,pZ1't%1p<p1&%1p<p1&%1p<p1&%1p<p1&%1p<p1&pUt!pU",
"t!pUt!pUt%6pF$PpK&'sq;&!,pPt&#Ap;&$6p;#;p'6p6!;p;&!;p;!;p;&!;p;!1pF$KpA&'pPt!'pPt&$,FjpZ#'pPt!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6",
"$'jo`%Pp,&$,pU&!UoZ$`oP%Po`!Pp'&!,pP!eoet!Up,t!eoe!ZseK'&!;p;&!,PsoZtFp(pF&%Fp(pF&%Fp(pF&%Fp(pF&%Fp(pF&pUt!pUt!pUt!pUt%6",
"pF$,pj'%'peWpe'&,pPt&#Ap;&$6p;$joF6p6!;p;&!;p;!;p;&!;p;!1pF$,pe&!Zoj't!Zoj't&%,`p'#Zoj't6pA&!Ap6!6pA&!Ap6!6pA&!Ap6%Pp1$j",
"oZ&%Kp;&!UoZ$`oP%Po`!Pp'&!,pP!eoet!;pKt!eoe!ZreP6&#;p;&#FjsZtZo`#`oZ&%Zo`#`oZ&%Zo`#`oZ&%Zo`#`oZ&%Zo`#`oZ&pUt!pUt!pUt!pUt",
"%6pF%PpK%'pP$PpK&,pPt&#Ap;&$6p;$Po`6p6!;p;&!;p;!;p;&!;p;!1pF%Pp;&ApjAtApjAt&%6pA#ApjA&%6pA&!Ap6!6pA&!Ap6!6pA&!Ap6%1pP#;p",
"A&%joj&#UoZ$`oP%Po`!Pp'&!,pP!eoet#jojt!eoe!;CFZpF&!;p;&'`ojPFC;tjoP#Pp'&$joP#Pp'&$joP#Pp'&$joP#Pp'&$joP#Pp'%pUt!pUt!pUt!",
"pUt%6pF%'joj'$'pP$'jp1%,pPt&#Ap;&$6p;$6p;p6!;p;&!;p;!;p;&!;p;!1pF%,p`&'ZqjUA,&!'ZqjUA,t&!joU#'ZqjUA,&6pA&!Ap6!6pA&!Ap6!6",
"pA&!Ap6&`oj'!Uoj'&$6pK&#UoZ$`oP%PoZ!Pp,&!,pP!eoet#Kp6teoe&#Uoe&!;p;&;p;&#*%6p;#;p6&#6p;#;p6&#6p;#;p6&#6p;#;p6&#6p;#;p6%p",
"eYW;#peYW;#peYW;#peYW;&6pF&KpK$'pP%;p`%,pPt&#;pA&$6p;%eoPp6!;p;&!;p;!;p;&!;p;!1pF&Up6&!AjsU,&!AjsU,$Zoj&#'joZ%AjsU,$6pA&",
"!Ap6!6pA&!Ap6!6pA&!Ap6&Ap;'pP&%Up1&#UoZ$Uoe%eoP!Fp;&!;pF!eoet#1pUteoe&#Fp,&;p;&Kp,&#;p;%Kp'#'pK&#Kp'#'pK&#Kp'#'pK&#Kp'#'",
"pK&#Kp'#'pK%ssoK#ssoK#ssoK#ssoK&6pF&'joj'#'pP&UpF$,pPt&#1pZ&$6p;%Kojp6!;p;&!;p;!;p;&!;p;!1pF&6pZ&#'K`re,&!'K`re,#Pp,&!Ap",
"P&'K`re,#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&'joZ;p6&$'p`&$UoZ$Ap;#;pA!;pU&!Zp1!eoet$`p'&%eoe&#;p;&;p;&Up'&#;p;%`oe%eo`&#`oe%eo`&",
"#`oe%eo`&#`oe%eo`&#`oe%eo`%pj_]A#pj_]A#pj_]A#pj_]A&6pF&!KpK#'pP&'jp1#,p`JI1&$ep`J1!6p;%1r6!;p;&!;p;!;p;&!;p;!1pF&!`p1&%,",
"FjpZ&%,FjpZ#;pZ&!`p;&$,FjpZ#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&!PpZoe&%ApA&$UoZ%`pfp`#,q,%,pj#eoet$ApA&%eoe&#Ap6&;p;&Kp1&#;p;$'p",
"P%Pp'&'pP%Pp'&'pP%Pp'&'pP%Pp'&'pP%Pp'$pUt!pUt!pUt!pUt%6pF&!'joj'!'pP&!;p`#,sspK&$6sqK!6p;&`q6!;p;&!;p;!;p;&!;p;!1pF&!;pU",
"t!,`p't,`p'#`pjU;6PjpZt!,`p'!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&!1rK&%`p'&$Foe%'UrU'$KqU7UqK#eoet$'p`&%eoe&#Fp1&;p;&Ap;&#;p;$6pA",
"*Ap6&6pA*Ap6&6pA*Ap6&6pA*Ap6&6pA*Ap6$pUt!pUt!pUt!pUt%6pF&#KpK!'pP&#ZpF!,sspK&%,UesK!6p;&Fq6!;p;&!;p;!;p;&!;p;!1pF&#ep,t!",
"6pAt!6pA#,`sqe,t#6pA!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&#`q,&$1pU&%6p&!'7'&!ZsqF$eoet%Up1&$eoe&#Pp,&;p;&1pF&#;p;$KssK&KssK&KssK&",
"KssK&KssK$pUt!pUt!pUt!pUwt%;p;&!;p;!;p;&!;p;u&$joUt#joU$'KerjFt&joU!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&#ApZ&%Kp6&%'p6t&!AesK%eoe",
"t%6pK&$eoe&#Up&!;p;&'pP&#;p;$`ss`&`ss`&`ss`&`ss`&`ss`$pUt!pUt!pUt!pUwt%6pK&!Kp6!6pK&!Kp6t&#Zoj&#'joZ!Zoj&#'joZ&!f6'&!Zoj",
"&#'joZ!1pK&!Kp1!1pK&!Kp1!1pK&!Kp1&#ApA&%joet!`o`t&$'1Fp`%eoet&eoj&$eoe&#Zp'&;p;&'pZ&#;p;#'pU51Up'$'pU51Up'$'pU51Up'$'pU5",
"1Up'$'pU51Up'#pUt!pUt!pUt!pUu'-'t#'-'t#'.t%.'t!'-'&'pe&!ep'!'pe&!ep'&#'-'&!Pp,&!ApP!Pp,&!ApP&1pZF&!Pp,&!ApP!'pe&!ep'!'pe",
"&!ep'!'pe&!ep'&#`oj'&$;pKt!ApKuUpA$eoet&Kp;&#eoe&#Pp;&;p;&;pK&#;p;#;pA&!;p;$;pA&!;p;$;pA&!;p;$;pA&!;p;$;pA&!;p;#pUt!pUt!",
"pUt!pUuep6t#ep6t#UpAt$6p`t#ep6&!ep1%1pe$ep1%1pe&$ep6&!;pZ&!`p;!;pZ&!`p;&'1Ajo`&;pZ&!`p;#ep1%1pe$ep1%1pe$ep1%1pe&#1pP&%Up",
",t#Fp`6't&#,joj'#eoe/'t,pU#'/eoe&#ApeF1,#;p;#,1FepA&#;p;#Pp,&!'pP$Pp,&!'pP$Pp,&!'pP$Pp,&!'pP$Pp,&!'pP#pUt!pUt!pUt!pUt&%K",
"pKt#KpKt#;pZt$'eoj,t!KpK&#;qU<Uq;$;qU<Uq;&#KpK&$ZpjU;6PjpZ$ZpjU;6PjpZ&%Fp1&ZpjU;6PjpZ$;pjA16Uq;$;pjA16Uq;$;pjA16Uq;&#Kp6",
"&$'p`t%Krh't$FpP#esoKt!`p'!Ksoe&$Zs,!;p;!,sU'&#;p;#eoe&$eoe$eoe&$eoe$eoe&$eoe$eoe&$eoe$eoe&$eoe#p`JI6!p`JI6!p`JI6!p`JI6&",
"$1p`t#1p`t#'joe't$PpAt!1p`&%PsqP&PsqP&#1p`&%,esqe,$,esqe,&!FKUjo`&!,esqe,%UsqU&UsqU&UsqU&$joe&%FpAt&;Pes't%ep6!esoKt!ApF",
"!Ksoe&$'Fjq,!;p;!,qeK&%;p;!,pP&$Pp,!,pP&$Pp,!,pP&$Pp,!,pP&$Pp,!,pP&$Pp,!sspP!sspP!sspP!sspP&$`oj,t#`oj,t#Up6t$6pUt#`oj,t",
";ere;&#;ere;&$`oj,t!FjrjF&#FjrjF&$epeP,&$FjrjF&#AereA&#AereA&#AereA&$;pF&%17't&$,;Ct&1B6!:7,t!'71!,:7t!,7'!,7,!'7't!,7,!",
"ApA&$;pA!ApA&$;pA!ApA&$;pA!ApA&$;pA!ApA&$;pA!sspP!sspP!sspP!sspP&#'2,t#'2,t$2,t%,2't!'2,t$'2't!'2'&%'2,t%'16't!'16't)t$'",
"16't!'2't!'2't!'2't6B'xxxxwvu&!,LAt&AKF'&!6L6&#6'%'6&#2%1,t#AKF'&#AK1%1KAt`g&%'FKA%AKF'$1K;%'L&$'1&#iifK#AKF'$6L6&#6L6tP",
"kZ't$2%1,$AK1%1KA#ZiiZ&!-&%6L1t&!6L1t%AKF'%'FKA%AKF'%;JH;t!AKF'&'FKA%AKF'$;iih;&!.t'FKA%AKF'&;JH;$6f;&!;f6&$6L6t%6L6t&%P",
"p;t$;pK&!6qj6&`oj,#1p`%'`pe1!1o`t!;pK&$PoZ%ZoP&%,r1&%ApP#PpF%;oj'$Aoj&#;pe'&sspP#1jo`'!;pZ&#6qj6&$PrZ't'`pe1!1o`$PoZ%ZoP",
"#esse&PpP&#,jo`t&!,joZt%;pK&!ApP#PpF&`sq`t;pK&#ApP#PpF%;ssr;&!eoet!ApP#PpF&!`sq`$6pA&!Ap6&#6qj6t#6qj6t&%`oj1t!,joZ&!6pao",
"j6$,qF#Fq,$FoePojA`oKt,joZ&%6pF(Fp6&%AokoFtApPUpF&'joU1';eoP&#Zq;&peON;$1joeFpZ&#6paoj6&!PpK;pZ'&%FoePojA`oK$6pF(Fp6#FMZ",
"pZMF%'r'&!`oj,t&'`oe't$,joZ&$ApPUpF&!UdbU&%,joZ&%ApPUpF&1OZpZO1&!eoet#ApPUpF&#UdbU$6pA&!Ap6&!6paoj6t6paoj6t&$'eoe't`oe'&",
"6jo`(Zoj6$jp6#6pe%Zo6!;jpj,t`oe'tKsoKtUo[oZt!ArF&#6js`'&#Fq'&pUt%1jqZ&#6jo`(Zoj6%PpF#6joZ'&$Zo6!;jpj,%KsoK&#ApA&%ZpZ&!Pp",
"6t&!Pp1t%`oe'&%ArFu&!`oe'tArFt#;p;t#eoet$ArFu!6pA&!Ap6&6jo`(Zoj6&$6jo`(Zoj6t&$1UZA&%;[1&'UZP'#'PZU'#'A1%1A'%;A'#,B't;[1t",
"#1PaP1t!joLojt#A[A&%,KZ`UAt6A,&!pUt&1UZP&#'UZP'#'PZU'#'71%,7,&$;A'#,B'&!1PaP1&$ApA&%'<'&,UZAt&!1[;t%;[1t!A[Au&!;[1t#A[At",
"$;p;t#eoet%A[Au#6pA&!Ap6%'UZP'#'PZU'&!'UZP'#'PZU'wut&$1p<p1v&%pUw&%ApAw&#-v&;p;&#6NjojOK6ut$6pA&!Ap6wvut#Fp(pFv&%pUw&%Ap",
"Aw%1ZjpjZ1v#;p;&#PssqPt&$6f;&!;f6!6pA&!Ap6v&#ZiiZ!ZiiZ!ZiiZ!ZiiZ!6gA&6f6!Uf,&$'fU&#`gt!Zo`#`oZ&!1ieUF1&iifK!iifK!pUt!iif",
"K!1fF&!Ff1&1fAt!ZiiZ!ZiiZ&ApA&ZiiZ!'fKt!'ig6&$6gA&6f6!6gA&6f6$KsqK$1ifUF1&1ifUF1t!;p;&#KmqnjP!;iih;!6pA&!Ap6!6pA&!Ap6!;f",
",$AfK%f;!Uf,&$'fUt!esse!esse!esse!esse!6q`&;p6!6pP&$Kp6&!,r1tjoP#Pp'&6srU%sspP!sspP!pUt!sspP!1pF&!Fp1&6pFt!esse!esse&ApA",
"&esse!,pPt!,sq;&$6q`&;p6!6q`&;p6#6q`B`q6#1ssP%1ssPt;p;t#eoe&$;ssr;!6pA&!Ap6!6pA&!Ap6!1p6$KpZ$,p1!6pP&$Kp6t!FMZpZMF!FMZpZ",
"MF!FMZpZMF!FMZpZMF!6r,%;p6#`p'&!'jo`&#AokoF&%6p;#;p6&6pZMUjpZ'#peON;!peON;!peYW;#peON;!1pF&!Fp1&6pFt!FMZpZMF!FMZpZMF&ApA",
"&FMZpZMF!,pPt!'OZp;&$6r,%;p6!6r,%;p6#ep6%6pe#1pZMPeqP$1pZMPeqP&%;p;t#eoe&$1OZpZO1!6pA&!Ap6!6pA&!Ap6!,p;$Uoj`$1p,#`p'&!'j",
"o`t&!ApA&%ApA&%ApA&%ApA&6pjoK%;p6#;pK&!Fp;&#Uo[oZ&%Kp'#'pK&6pF%'UpK#pUt!pUt!ssoK#pUt!1pF&!Fp1&6pFt&ApA&%ApA&%ApA&%ApA&,p",
"Pt&#Ap;&$6pjoK%;p6!6pjoK%;p6!'pe&!ep'!1pF&6ep;#1pF&6ep;&$;p;t#eoet%;p;&#6pA&!Ap6!6pA&!Ap6#pF$`j`j$;p$;pK&!Fp;t&!ApA&%ApA",
"&%ApA&%ApA&6pUoe%;p6$`oj'&eo`&$joLoj&%`oe%eo`&6pF&'eo`#pUt!pUt!pj_]A#pUt!1pF&!Fp1&6pFt&ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6pU",
"oe%;p6!6pUoe%;p6!1pK&!Kp1!1pF&!ApP#1pF&!ApP&$;p;t#eoet%;p;&#6pA&!Ap6!6pA&!Ap6#eoK$jeZo'#Foe%`oj'&eo`t&#ApA&%ApA&%ApA&%Ap",
"A&6p;p1$;p6$;pF%Ap;&#1p<p1&#'pP%Pp'%6pF&!Kp'!pUt!pUt!pUt!pUt!1pF&!Fp1&6pF!,AVK1&$ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p;p1$;p6",
"!6p;p1$;p6!;p;&!;p;!1pF&!'p`#1pF&!'p`&$;p;t#eoet%;p;&#6pA&!Ap6!6pA&!Ap6#ZoU#'oZUo1#KoZ%;pF%Ap;t&#ApA&%ApA&%ApA&%ApA&6p1`",
"oP$;p6%eoj'$eoe&$Fp(pF&#6pA*Ap6%6pF&!Ap;!pUt!pUt!pUt!pUt!1pF&!Fp1&6pF;soF&#ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p1`oP$;p6!6p1`",
"oP$;p6!;p;&!;p;!1pF&#ep#1pF&#ep&$;p;t#eoet%;p;&#6pA&!Ap6!6pA&!Ap6#UoZ#1oPKo;#UoP&eoj'$eoet&$ApA&%ApA&%ApA&%ApA&6p6Foj$;p",
"6%ApA#;pA&$Zo`#`oZ&#KssK%6pF&!;p;!pUt!pUt!pUt!pUt!1pF&!Fp1&6pFeoZUr6&!ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p6Foj$;p6!6p6Foj$;p",
"6!;p;&!;p;!1pF&!,pZ#1pF&!,pZ&$;p;t#eoet%;p;&#6pA&!Ap6!6pA&!Ap6#Koe#;oFAoF#`oF&ApA#;pAt&$ApA&%ApA&%ApA&%ApA&6p6,p6#;p6&eo",
"e#`oe&%joP#Pp'&!`ss`%6pF&!;p;!pUt!pUt!pUt!pUt!1pF&!Fp1&6peP,#'Kpe&!ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p6,p6#;p6!6p6,p6#;p6!;",
"p;&!;p;!1pF&!FpA#1pF&!FpA&$;p;t#eoet%;p;&#6pA&!Ap6!6pA&!Ap6#Ap#Fo;6oK#eoA&!eoe#`oet&%ApA&%ApA&%ApA&%ApA&6p;!ZoU#;p6&Ap;6",
"pA&$6p;#;p6&'pU51Up'$6pF&!;p;!peYW;#peYW;#pUt!peYW;#1pZOKZp1&6q,%'q'&ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p;!ZoU#;p6!6p;!ZoU#;",
"p6!;p;&!;p;!1pF%,Fjp,#1pF%,Fjp,&$;p;t#eoet%;p;&#6pA&!Ap6!1pK&!Kp1#6p'!Po1,oU#p6&!Ap;6pAt&%ApA&%ApA&%ApA&%ApA&6p;!Aoj'!;p",
"6&'jo`Zoj'&$Kp'#'pK&;pA&!;p;$6pF&!;p;!ssoK#ssoK#pUt!ssoK#1ssp1&6p`&!Up6&ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p;!Aoj'!;p6!6p;!A",
"oj'!;p6!;p;&!;p;!1pe]`rK$1pe]`rK&%;p;t#`oet%;p;&#6pA&!Ap6!'pe&!ep$,p1!Zo(o`!,p,&!'jo`Zoj't&%ApA&%ApA&%ApA&%ApA&6p;!'pA!;",
"p6&!FrF&%`oe%eo`&Pp,&!'pP$6pF&!;p;!pj_]A#pj_]A#pUt!pj_]A#1pnlp1&6pK&!Ap6&ApA&%ApA&%ApA&%ApA&,pPt&#Ap;&$6p;!'pA!;p6!6p;!'",
"pA!;p6!;p;&!;p;!1sre6%1sre6t;p;t#Pp,t$;p;&#6pA&!Ap6#ep1%1p`$'p6!ej#k!1p&$FrFuApA&%ApA&%ApA&%ApA&6p;#UoZ!;p6&!'jpj'&$'pP%",
"Pp'%eoe&$eoe$6pF&!;p;!pUt!pUt!p`JI6!pUt!1pF&!Fp1&6pF&!Ap6&ApA&%ApA&AHUpUHA&ApA&,pPt&#Ap;&$6p;#UoZ!;p6!6p;#UoZ!;p6!;p;&!;",
"p;!1peWjp,&1peWjp,t!;p;t#6peKI1&#;p;&#6pA&!Ap6#;qU<UqA%joA'o`#`o';oe&$'jpj'uApA&%ApA&%ApA&%ApA&6p;#;p'6p6&#KpK&%6pA*Ap6$",
",pP&$Pp,#6pF&!;p;!pUt!pUt!sspP!pUt!1pF&!Fp1&6pF&!Ap6&ApA&%ApA&esse&ApA&,pPt&#Ap;&$6p;#;p'6p6!6p;#;p'6p6!;p;&!;p;!1pF$KpA",
"&1pF$KpAt!;p;t$ZspF&#;p;&#6pA&!Ap6$PsqK&`oK,oU#Uo1FoZ&%KpKu!ApA&%ApA&%ApA&%ApA&6p;$joF6p6&#;p;&%KssK$ApA&$;pA#6pF&!;p;!p",
"Ut!pUt!sspP!pUt!1pF&!Fp1&6pF&!Ap6&ApA&%ApA&esse&ApA&,pPt&#Ap;&$6p;$joF6p6!6p;$joF6p6!;p;&!;p;!1pF$,pe&1pF$,pet!;p;t%;jsF",
"&#;p;&#6pA&!Ap6%;esP&!UoP6oP#Ko6KoU&%;p;u!ApA&%ApA&%ApA&%ApA&6p;$Po`6p6&#;p;&%`ss`t%'ZoK$6pF&!Ap;!pUt!pUt&%PoU$pUt!1pF&!",
"Fp1&6pF&!Ap6&ApA&%ApA&$'ZoKtApA&,pPt&#Ap;&$6p;$Po`6p6!6p;$Po`6p6!6pK&!Kp6!1pF%Pp;%1pF%Pp;t!Po1t%'oUt#;p;&#1pK&!Kp1&!'6jo",
"K&#KoZAoF#FoAUoK&%;p;u!ApA&%ApA&%ApA&%ApA&6p;$6p;p6&#;p;&$'pU51Up't$ZoP%6pF&!Kp'!pUt!pUt&$Ko`%pUt!1pF&!Fp1&6pF&!Ap6&ApA&",
"%ApA&$ZoPt!ApA&,pPt&#Ap;&$6p;$6p;p6!6p;$6p;p6!'pe&!ep'!1pF%,p`%1pF%,p`t!jo`K't#AojZ;t;p;&#'pe&!ep'&#PoZ&$Fo`Ko;#;oKZoA&%",
";p;u!ApA&%ApA&%ApA&%ApA&6p;%eoPp6&#;p;&$;pA&!;p;t#1p1%6pF&!`oe#pUt!pUt&#'joA%pUt!1pF&!Fp1&6pF&!Ap6&ApA&%ApA&#1p1t!ApA&,p",
"Pt&#;pA&$6p;%eoPp6!6p;%eoPp6#ep1%1pe#1pF&Up6$1pF&Up6t1;`oj,t!'6FpP&%;p;&$ep1%1pe&#'p;&$;ojUo1#1oUeo6&%;p;u!ApA&%ApA&%ApA",
"&%ApA&6p;%Kojp6&#;p;&$Pp,&!'pPt#;pA%6pF&PpP#pUt!pUt&#,pP%pUt!1pF&!Fp1&6pF&!Ap6&ApA&%ApA&#;pAt!ApA&,pPt&#1pZ&$6p;%Kojp6!6",
"p;%Kojp6#;qU<Uq;#1pF&6pZ$1pF&6pZt#,pFt%Uoj&%;p;&$;pjA16Uq;&#,pP&$1p`o'#'oZjo,&%;p;t&#AHUpUHA!AHUpUHA!AHUpUHA!AHUpUHA!6p;",
"%1r6&#;p;&$eoe&$eoet$ep`ZA!6pUCPep`'#p`JI6!p`JI6&$UpeZK!p`JI6!1pF&!Fp1&6pF&!Ap6!AHUpUHA!AHUpUHA%ep`ZA%AHUpUHA!,p`JI1&$ep",
"`J1!6p;%1r6!6p;%1r6$PsqP$1pF&!`p1#1pF&!`p1&$;KPep,t!LZpP&%;p;&%UsqU&%UpeZK&'pk%kp'&%;p;t&#esse!esse!esse!esse!6p;&`q6&#;",
"p;&#,pP&$Pp,t#1PepK!6srZ'$sspP!sspP&$,K`pZ!sspP!1pF&!Fp1&6pF&!Ap6!esse!esse%1PepK%esse!,sspK&$6sqK!6p;&`q6!6p;&`q6%;ere;",
"%1pF&!;pU#1pF&!;pU&$KpjU;t!'q`K'&%;p;tAereAt,KepZ&!jp`%epjt;p;t&#esse!esse!esse!esse!6p;&Fq6&#;p;&#ApA&$;pAt&)!6sjZK;'%s",
"spP!sspPt!)!sspP!1pF&!Fp1&6pF&!Ap6!esse!esse&#)%esse!,sspK&%,UesK!6p;&Fq6!6p;&Fq6&!'2'&!1pF&#ep,!1pF&#ep,&#)t&)t#;p;t#'2",
"'t&)&!epU%Zp`t;p;xxxvt$6'%'6t$AKF'&%-&$'FKA%AKF'&#'2,&%,KZg'!1f,!;f6t%;JH;&1JI'$;JH;#6gF&6f6ut&#PUt%-%;KA$'LFt&#;L,&$1L;",
"t16'$7&!1'&1&!,LAt&#AL'&$6L6&%1'%'1&!,LAt&#AL'&$6L6t7'#'61&!1'%'1&!,LAt&#AL'&$6L6&%1'%'1u`oj,#1p`t!;pK&%PpP&$ApP#PpF&'Pj",
"sU1&Ajs'!6p,!Ap;t%`sq`&Ksr'$`sq`#6qe&;p6&'FejP1&%,KfK,t!`ot!6`jq`6,pA%ApKt&1pU&$1jqA&$`pj;!'oj&Pp6#'joe&!PpAt&;pK&$;r;&#",
"`oj,#1joZ&!PpAt&;pK&$;r;&#,epe1!1o`&`oj,#1joZ&!PpAt&;pK&$;r;&#`oj,#1joZt&$,qF#Fq,&%,joZ&%'r'&$ApPUpF&,jsr6$1sp'!6p,!Ap;t",
"%UdbU&Adc'$UdbU#6r6%;p6&ZoZPk'&$eoVoet!`otKsqjo`&!Pp;t$'eo`'&#1joeUpA&!;ojKjoKZoU&qU#;q6&!Zp1t$,joZ&$;p[p;&,qF#Fq,&!Zp1t",
"$,joZ&$;p[p;&!FoePojF`oF%,qF#Fq,&!Zp1t$,joZ&$;p[p;&,qF#Fq,t&$jp6#6pet`oe'tZpZtArF&!`p`F2AUpj'#Zsp'!6p,!Ap;vt&6pjoU%;p6%,",
"oU#AoA&#6oF#Ko6t`o&%;q`A;Kjq6&#Zoj,t#Zoj,&#1joe'!PpA&PoA!1epj6&`pA#,q'&!'eoe't!'`oe'&#;pZ(Zp;&jp6#6pe&#'eoe't!'`oe'&#;pZ",
"(Zp;&Zo6!6jpj,&jp6#6pe&#'eoe't!'`oe'&#;pZ(Zp;&jp6#6pet&%'A1%1A'&%;[1t!'<'t!A[A&!'pe&!Zp;#jsp'!6p,!Ap;vt&6pKp,$;p6%;oF#,o",
"U&#Fo6#;oF&$6KjoUA,&!ep6%6pj&$'VFt!1V6&#'PUP'$FV1%1;,#';A'&!'A6%1F,&$1VAt!;V1&#'PUP'$KUP'%,A1%1A,&$1VAt!;V1&#'PUP'$KUP'%",
"6;'#,A;&#,A1%1A,&$1VAt!;V1&#'PUP'$KUP'%,A1%1A,vut&$6pK&!'-'!'sq'!6p,!Ap;!,AVF,v&$6p6joF$;p6%'j`'!Po6&#,oU(Uo,&!,`sojF%'p",
"e&Aq'xxu&#'pUt!'sq'!6p,!Ap6Fsj;&%`g&#iifK!ZiiZ!6p1Poe$;p6&Kpjoe&%ZokoZ&!'jqjoeqF$6pK%'jq6xwv&Uf,&$'fU!6iif,!6iif,!6iif,#",
"Zoj;t'sq'!6p,!ApAjo[r,&#,r1&!sspP!esse!6p61p;#;p6&!1KP6t!1Q1&#PpZ,`o!Ajoj,#;p;%Kr;&'AP[PF'&#'AP[PF'&#'AP[PF'&#'AP[PF'&#'",
"AP[PF'&$;KZUF1&%;KZUF1&%;KZUF1&%;KZUF1&%;FVF;&%;FVF;&%;FVF;&%;FVF;&%;FVF;&,L1&!1L,!,L1&!1L,!,L1&!1L,!,L1&!1L,t%6pP&$Kp6!",
"6ssp,!6ssp,!6ssp,#;jp`PA1&#esp'!6p,!ApeP,#'KpU&#AokoF&!peON;!FMZpZMF!6p6!`oZ#;p6ut!'pj#`o#KpA#;p;$,joZp;%;jsojA&;jsojA&;",
"jsojA&;jsojA&;jsojA&1`so`'&1`so`'&1`so`'&1`so`'&1`so`1&1`so`1&1`so`1&1`so`1&1`so`1$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&",
"`p'&!'jo`#1ONep,!1ONep,!1ONep,$,UsjP'%Usp'!6p,!Apj&!`oj&#Uo[oZ&!pUt&ApA&6p;!Ap,!;p6%'AP[PF'%,L1&!1L,!1pP#`o#1kK#;p;$Uoe;",
"p;$;qeVjq6$;qeVjq6$;qeVjq6$;qeVjq6$;qeVjq6$,jpjQjpe,$,jpjQjpe,$,jpjQjpe,$,jpjQjpe,$,jqfqj,$,jqfqj,$,jqfqj,$,jqfqj,$,jqfq",
"j,#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&;pK&!Fp;t!,pZt!,pZt!,pZ%6Zpere,$,jso'!6p,!ApU&!Pp,&!joLoj&!pUt&ApA&6p;!'joK!;p6$",
";jsojA$6pA&!Ap6!;pA#`o&$;p;#1p6;p;$ep6%Ap`$ep6%Ap`$ep6%Ap`$ep6%Ap`$ep6%Ap`$UpA'#'ApZ$UpA'#'ApZ$UpA'#'ApZ$UpA'#'ApZ$UpP1#",
"1PpU$UpP1#1PpU$UpP1#1PpU$UpP1#1PpU$UpP1#1PpU#6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&!`oj'&eo`t#Zp,t!Zp,t!Zp,$KpA'#1ZpU%6es",
"'!6p,!Ap;&!Fp1&1p<p1&pUt&ApA&6p;#Koj!6p6#;qeVjq6#6pA&!Ap6!;p;#`o&$;p;#`oZ!;p;#'VK&!jp'!'VK&!jp'!'VK&!jp'!'VK&!jp'!'VK&!j",
"p'!,p`&!eoj#,p`&!eoj#,p`&!eoj#,p`&!eoj#,pj&!jp,!,pj&!jp,!,pj&!jp,!,pj&!jp,!,pj&!jp,!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t",
"&!;pF%Ap;t!;pKt!;pKt!;pK%eo`&!Uoj'&;K`p'!6p,!Ap;&!Fp6&Fp(pF&pUt&ApA&6p;#,p;6p6#ep6%Ap`#6pA&!Ap6!;p;#`o&$;p;!;p1!;p;t#Kp1",
"t!Kp1t!Kp1t!Kp1t!Kp1!6pF&!Kp6!6pF&!Kp6!6pF&!Kp6!6pF&!Kp6!6pP&!Pp6!6pP&!Pp6!6pP&!Pp6!6pP&!Pp6!6pP&!Pp6!6pA&!Ap6!6pA&!Ap6!",
"6pA&!Ap6!6pA&!Ap6t&#eoj'$eoet#eoj't!eoj't!eoj'$1pF&!6pA&#;p'!6p,!Ap;&!Fp6&Zo`#`oZ&pUt&ApA&6p;$ZoZ1p6!'VK&!jp'!6pA&!Ap6!;",
"p;#`o&$;p;!eoP#;p;t#Fp6t!Fp6t!Fp6t!Fp6t!Fp6!Ap1&!1pF!Ap1&!1pF!Ap1&!1pF!Ap1&!1pF!Ap6&!6pA!Ap6&!6pA!Ap6&!6pA!Ap6&!6pA!Ap6&",
"!6pA!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&#ApA#;pAt!FpAt!FpAt!FpA%6pF&!6pF&#;p'!6p,!Ap;&!Fp6&joP#Pp'%pUt&ApA&6p;$;p;p6t!",
"Kp1!6pA&!Ap6!;p;#`o&$;p;Foj'#;p;%1KZnq6$1KZnq6$1KZnq6$1KZnq6$1KZnq6!FpZYUZpF!FpZYUZpF!FpZYUZpF!FpZYUZpF!Fp1&!1pF!Fp1&!1p",
"F!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&$eoe#`oet!,jo`t!,jo`t!,jo`&'p`&!Pp'&#;p'!6p,!Ap;&!Fp6%",
"6p;#;p6%peYW;&!ApA&6p;%eoUp6t!Fp6!6pA&!Ap6!6pF#`o#'B6#;pAjoF$;p;$6jss6#6jss6#6jss6#6jss6#6jss6!FsspF!FsspF!FsspF!FsspF!F",
"p1&!1pF!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&$Ap;6pAt!Up1t!Up1t!Up1&!ZpU,$1joU&$;p'!",
"6p,!ApF&!Kp1%Kp'#'pK%ssoK&!ApA&6p;%Fojp6$1KZnq6!6pA&!Ap6!,p`#`o#;pK#;peoe%;p;#6pjP6/Kp6!6pjP6/Kp6!6pjP6/Kp6!6pjP6/Kp6!6p",
"jP6/Kp6!FpKJI1!FpKJI1!FpKJI1!FpKJI1!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6t&$'",
"jo`Zoj't6pPt!6pPt!6pP&#'eqeU`o`;&%;p'!6p,!Ap`&!Up'%`oe%eo`%pj_]A&!ApA&6p;%'r6#6jss6!6pA&!Ap6#ep1!`o#Zp6#6r;%Kp6#KpA&!Fp6",
"!KpA&!Fp6!KpA&!Fp6!KpA&!Fp6!KpA&!Fp6!Fp,t!Fp,t!Fp,t!Fp,t!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!Fp1&!1pF!6pA&!Ap6!6pA&!Ap6!",
"6pA&!Ap6!6pA&!Ap6t&%FrFt!`oj't!`oj't!`oj'&$,PsoeA&%;p'!6p,!Aq,%,joe%'pP%Pp'$pUt&ApA&6p;&Uq6!6pjP6/Kp6!6pA&!Ap6#;qPeoAep`",
"$'q`&ep,#`oj'&!Kp6!`oj'&!Kp6!`oj'&!Kp6!`oj'&!Kp6!`oj'&!Kp6!;p;t!;p;t!;p;t!;p;t!;pA&!Ap;!;pA&!Ap;!;pA&!Ap;!;pA&!Ap;!;pA&!",
"Ap;!1pP&!Pp1!1pP&!Pp1!1pP&!Pp1!1pP&!Pp1t&%'jpj'tFpAt!FpAt!FpAt!'AKZjqK&$;p'!6p,!ApPjP16PjpA%6pA*Ap6$pUt&ApA&6p;&6q6!KpA&",
"!Fp6!6pA&!Ap6$Psqe,%`p;%1pj$joj&#jp6!joj&#jp6!joj&#jp6!joj&#jp6!joj&#jp6!1pZ&!'2'!1pZ&!'2'!1pZ&!'2'!1pZ&!'2'!1p`&!`p1!1p",
"`&!`p1!1p`&!`p1!1p`&!`p1!1p`&!`p1!'pe&!jp'!'pe&!jp'!'pe&!jp'!'pe&!jp'uKpKt'jo`t!'jo`t!'jo`t&!'Kjp1&#;p'!6p,!Ap;Zso`&KssK",
"$pUt&ApA&6p;&!`p6!`oj'&!Kp6!1pP&!Pp1%;`rjP'&epjA1;UqA$Zp;&Aq6!Zp;&Aq6!Zp;&Aq6!Zp;&Aq6!Zp;&Aq6!'jp'%'`p,!'jp'%'`p,!'jp'%'",
"`p,!'jp'%'`p,!'jp,%,pj#'jp,%,pj#'jp,%,pj#'jp,%,pj#'jp,%,pj$ep6%6pe$ep6%6pe$ep6%6pe$ep6%6peu!;p;tPp6t!Pp6t!Pp6t&$KpA&#;p'",
"!6p,!Ap;,PjqK&!`ss`$pUt&ApA&6p;&!Kp6!joj&#jp6!'pe&!jp'&!'eo,&#AssU%ApeK1'AUjUp6!ApeK1'AUjUp6!ApeK1'AUjUp6!ApeK1'AUjUp6!A",
"peK1'AUjUp6#ApjP1,AZpZ$ApjP1,AZpZ$ApjP1,AZpZ$ApjP1,AZpZ$;qU<Uq;$;qU<Uq;$;qU<Uq;$;qU<Uq;$;qU<Uq;$;qU7Uq6$;qU7Uq6$;qU7Uq6$",
";qU7Uq6u!;p;&%1pUt!1pUt!1pUt&%,pP&#;p'!6p,!Ap;$2&#'pU51Up'#pUt&ApAt&Zp,!Zp;&Aq6#ep6%6pe&$`o&#'joKAere;&!ZspKAp6#ZspKAp6#",
"ZspKAp6#ZspKAp6#ZspKAp6$Usqe'%Usqe'%Usqe'%Usqe'%PsqP&PsqP&PsqP&PsqP&PsqP&PsqP&PsqP&PsqP&PsqPu#;p;&%`oj,t!`oj,t!`oj,&%PpA",
"&!FpK&#;p'!6p,!Ap;t#;pA&!;p;#pUt&ApAt%,pj#ApeK1'AUjUp6#;qU7Uq6&$`o&#Koj'#'2'&%Fjq`K'Ap6$Fjq`K'Ap6$Fjq`K'Ap6$Fjq`K'Ap6$Fj",
"q`K'Ap6%;erjF'&!;erjF'&!;erjF'&!;erjF'&!6`r`6&#6`r`6&#6`r`6&#6`r`6&#6`r`6&#;ere6&#;ere6&#;ere6&#;ere6u$;p;&$ApFt!ApFt!Ap",
"FtApj6$'6ep6&#;p'!6p,!Ap;t#Pp,&!'pP#pUt&ApAt(1KepK$ZspKAp6$PsqP&%`o&#2,t&$,6't#,6't#,6't#,6't#,6't$'16,t!'16,t!'16,t!'16",
",t#-t$-t$-t$-t$-t#'2't!'2't!'2't!'2'u&;p;&#'joet!'joet!'joet#`qjajqU&$;p'!6p,!Ap;t#eoe&$eoe#p`JI6!AHUpUHA&!`s`&Fjq`K'Ap6",
"%;ere6t,1xwvut$;p;&#ApUJI1!ApUJI1!ApUJI1#,UsqP'&$;p'!6p,!Ap;t!,pP&$Pp,!sspP!esse&!`qjF&$,6't$'2'xx&#;p;&#FsspF!FsspF!Fss",
"pF%,AKQKA,t,B#,B'!)t#ApA&$;pA!sspP!esse&!;A;1'xxut&!;p;&#FsspF!FsspF!FsspFxxxxxx&;K;%,KFt!6L1&$,LAt#'1&%FL'$;L1%AK6%6KA&",
"%-&$'LF%FL'#,L1!,AVK1&!AK6%6KA&$6L,!'L;$;L#1FZUF't!AL'&$6L6&#'LF%FL'u#7'#'61&AK6%6KA&$6L,!'L;u&#',t&#-t%(t%',t$-t$-t$,'t",
"$'1t#;KUZPF'&#AL,t&#1L;&%'LFu%Foe%PoZt,joZ&$,jqKtAp`&$1jo`'!FpP&PoZ%ZoP&$PpP&$ApP#UpA$6pF;soF&PoZ%ZoP&#,joP#Zoe'$Up'Zse1",
"&%;pK&$;r;&#ApP#UpAt$Fp&,epe1!1o`&PoZ%ZoP&#,joP#Zoe'$6UjZ;'&!'`e6&!;epjU6t!6`jpj`6&$6UepeU6&$,Zjq`A&#1Zjpj`;&#1ZjpjZ1&#6",
"UepeP1&%1joj,&$,ZsojF&!1jo`t&!'eoe'&$'`qZu$,joP1'FjoA&%'`oe'&#,joePpK&%`q6&$1joeKpP&!6pF-Kp6&#'r'&$ApVpA%6pFeoZUr6%6pF-K",
"p6&!'eoZ#Koj1%Up;ojU`qe'&#,joZ&$;p[p;&#ApVpAt%Poe&FoePojF`oF&6pF-Kp6&!'eoZ#Koj1$Ps`&!PoP&!Ksp;&%PsqP&!AsqA&!AsqZ'%KsqU&K",
"sqK&;spj6&$PqF&#'jqeZjqA&!;pPt&Up1&$'`pKpZu$;jsPtPp1&#,eoe,!KpK&$Kpj'&%1jqP&$FsoF&%`p`tArA&6peP,#'Kpe&FsoF&#Uoe'!;p;&Up`",
"A'#,ZpF&!'`oe'&#;pZ(Zp;&#ArAt.#UoZ&Zo6!6jpj,&!FsoF&#Uoe'!;p;$'joF';jo1%6oe'&6peF1Pjp1&#FqZ<ZqA%FqZBZqF%1qeK;ZqP$6q`FAZqF",
"$6q`B`q6$;qZ;A`q6&#;q1&#UpU1#';q,&!FpAt$ApA&$'`oj6!;pZu$,K[P6t,V;&$KV,$AV1&$6A,t!1VKt1P[P1t'<'t!;VA&!6q,%'q'&1P[P1&#1V1#",
"PUA&!UpU&;pU&!;V1&#'PUP'$KUP'&#;VAt!eoe#ZoP&6;'#,A;&%1P[P1&#1V1#PUA%;oe$PoP$'eo6&!Kp;$;pK&!'joj,%,pj'$ep1%1pe%ZpF%'jp1#e",
"p6%,pj'#ep6%6pe$Zp1%ApU&$1F1&$q'&PpA&#KUP't!'PUF&%AV6$6V;wvt&$6p`&!Up6ut$UpA&'jo`vt!eoe#`oFv&$KoU$Ao`$UoP&#Up'%jo`&!;pU&",
"!Up;#,p`&!`p,$joj'&PpA!'pe&!Zp6!'pe&!ep'!'p`&!eojt&$1pU&!1kKxu%6pK&!Ap6ut$Up'&!`oevt!eoe#;A,v&$KoU$Ao`#;oe'&#Kp6%;KF&!Fp",
"6&!6pF#;pA&!Ap;#,pU&!6pK!6pK&!ApF!1pK&!Kp1!1pF&!;Q't&#;pAvut&#'AP[PF'&$;KUZPF'&$;KUZPF'&$;KUZPF'&$;KUZPF'&$;KZUF1&%;KZUF",
"1&%;KZUF1&6pF&!Ap6&;FVF;&%;FVF;&#Up'&!FKF%1FP[PF,&#1FP[PF,&#1FP[PF,&%eoe&$,L1&!1L,!,L1&!1L,!,L1&!1L,!;oe$PoP!'jo1&$1pUt$",
"Pp&$pP#,pP&!Pp,#6pF&#-'!;p;&#-'!;p;&!;p;!'pPt#AOL'&!;p;t#AOL'&#AOL'&#AOL'u$;jsojA&,ZsojF&,ZsojF&,ZsojF&,ZsojF&1`so`'&1`s",
"o`'&1`so`'$6pF&!Ap6$1`so`1&1`so`1&Up't#PsqK&PsqK&PsqK$6NjojOK6!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6!'joF,Ajo1!UoKtPp6t#Pp&$pP$`oj&",
"!jo`$6pFt!;p;t!;p;&!;p;#Zoj't!esp'&!;p;t#esp'&#esp'&#esp'u#;qeVjq6$'jqeZjqA$'jqeZjqA$'jqeZjqA$'jqeZjqA$,jpjQjpe,$,jpjQjp",
"e,$,jpjQjpe,#6pF&!Ap6#,jqfqj,$,jqfqj,%Up't!Kq`L`qF$Kq`L`qF$Kq`L`qF#PssqP!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6#Ps`!Ao`t!'eoj,t!Pp&$",
"pP$ApU1#1UpA$6pFt!;p;t!;p;&!;p;#ApjAt`mq'&!;p;t#`mq'&#`mq'&#`mq'u#ep6%Ap`$UpU1#';q,#UpU1#';q,#UpU1#';q,#UpU1#';q,#UpA'#'",
"ApZ$UpA'#'ApZ$UpA'#'ApZ#6pF&!Ap6#UpP1#1PpU$UpP1#1PpU%Up't!joj,%1pj$joj,%1pj$joj,%1pj#KmqnjP!6pA&!Ap6!6pA&!Ap6!6pA&!Ap6$6",
"PjU;!,jo1t'6Up`t!Pp#,L,#pP%6epfpe6%6pFt!;p;t!;p;&!;p;#'ZqjUA,t!Pp'&!;p;t&#Pp't#Pp't#Pp'u!'VK&!jp'#q'&PpA#q'&PpA#q'&PpA#q",
"'&PpA!,p`&!eoj#,p`&!eoj#,p`&!eoj#6pF&!Ap6!,pj&!jp,!,pj&!jp,$Up't,pP&!AL'!,pP&!AL'!,pP&!AL'&!eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!",
"Ap6&%ZoFt6jsPtPp#PpP#pP&6jrj6&6pFt!;p;#6JF6!;p;&!;p;%AjsU,&%Pp'&!6pK&!'B6&#Pp't#Pp't#Pp'ut!Kp1!1pU&!1kK!1pU&!1kK!1pU&!1k",
"K!1pU&!1kK!6pF&!Kp6!6pF&!Kp6!6pF&!Kp6!6pF&!Ap6!6pP&!Pp6!6pP&!Pp6$Up't'pPt!'pPt!'pPt&!eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&$Ao",
"`t1qePZp;$'eje'!Pp#PpP#pP$,`qkq`'$6pFt!;p;#PsoK!;p;&!;p;&'K`re,&$Pp'&!,pe&!ApF&#Pp't#Pp't#Pp'ut!Fp6!;pAt!;pAt!;pAt!;pAt!",
"Ap1&!1pF!Ap1&!1pF!Ap1&!1pF!6pF&!Ap6!Ap6&!6pA!Ap6&!6pA$Up't!ZojA'tZojA'tZojA't%eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&#,k,!1P`F,",
"%`p;#'eoj1#Zp6#Pp#,L'#pP$epU6#6Upe$6pFt!;p;#PsoK!;p;&!;p;&$,FjpZ&$Pp'&#ep6&ep6&#Pp't#Pp't#Pp'u$1KZnq6!;p;t!;p;t!;p;t!;p;",
"t!FpZYUZpF!FpZYUZpF!FpZYUZpF!6pF&!Ap6!Fp1&!1pF!Fp1&!1pF$Up't!Frj`UF1&Frj`UF1&Frj`UF1&%eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&#`",
"oF!`sF#'pe%,joe'KpA$Pp&$pP#6pP&!Pp1#6pFt!;p;&!,pK!;p;&!;p;t,`p'&#Pp'&#;qZ;1KepZ&$Pp't#Pp't#Pp'u#6jss6!;p;t!;p;t!;p;t!;p;",
"t!FsspF!FsspF!FsspF!6pF&!Ap6!Fp1&!1pF!Fp1&!1pF$Up't#AespU'%AespU'%AespU'&#eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&!FoZ!FojF6Poe#",
"1pK&;p`pP%Pp&$pP#Pp1&!1pK#6pFt!;p;&!,pK!;p;&!;p;t!6pA&#Pp'&$Ksqe,&$Pp't#Pp't#Pp'u!6pjP6/Kp6!;p;t!;p;t!;p;t!;p;t!FpKJI1!F",
"pKJI1!FpKJI1!6pF&!Ap6!Fp1&!1pF!Fp1&!1pF$Up't%;FU`jqU&!;FU`jqU&!;FU`jqU&#eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&1oj,!UoU$eo6!6pA",
"&!KqZ&Pp&$pP#eo`&$`oe#6pFt!;p;&!,pK!;p;&!;p;t#joU&#Pp'&%;`rjK'&%Pp't#Pp't#Pp'u!KpA&!Fp6!;p;t!;p;t!;p;t!;p;t!Fp,t!Fp,t!Fp",
",u&!Fp1&!1pF!Fp1&!1pFu&!;ep,t;ep,t;ep,&!eoe&$6pA&!Ap6!6pA&!Ap6!6pA&!Ap6&`oA#`oF$UoK!1pA&!,q;&Fp6&!6pF#eoj&$joe#,pP&!1pP!",
"6pK&!;pF!6pK&!Kp6!Zoj&#'joZ&#Pp't!Zj6,t#Pp't#Pp't#Pp'u!`oj'&!Kp6!6pK&!'B6!6pK&!'B6!6pK&!'B6!6pK&!'B6!;p;t!;p;t!;p;u&!;pA",
"&!Ap;!;pA&!Ap;u&#Fp;t!Fp;t!Fp;&!eoe&$1pP&!Pp1!1pP&!Pp1!1pP&!Pp1%KoZ$`oA$UoK!'pZ&!`qj'%;pP&!Pp1#Pp;&!;pP$pj&!KpA!,pe&!Up6",
"!'pe&!ep'!Pp,&!ApP&#Pp't,p`F't!Pp't#Pp't#Pp'u!joj&#jp6!,pe&!ApF!,pe&!ApF!,pe&!ApF!,pe&!ApF!1pZ&!'2'!1pZ&!'2'!1pZ&!'2'&#'",
"-'&1p`&!`p1!1p`&!`p1%.t1L1&!6pA!1L1&!6pA!1L1&!6pA&!eoe&$'pe&!jp'!'pe&!jp'!'pe&!jp'$1oj'$UoK$`oA#jp,%UpFpZ%,pj'%'joj$;p`'",
"%'`p;$`p;&ep1#jp1%'jp,#ep1%1pe#;pZ&!`p;&#Pp't!1Aeoet!Pp't#Pp't#Pp'u!Zp;&Aq6#ep6&ep6#ep6&ep6#ep6&ep6#ep6&ep6!'jp'%'`p,!'j",
"p'%'`p,!'jp'%'`p,&#ep6&'jp,%,pj#'jp,%,pj%KpPt6pZ&!Up6!6pZ&!Up6!6pZ&!Up6&!eoe&%ep6%6pe$ep6%6pe$ep6%6pe$'eo;%6oe1!Aoj'#Fpj",
"P6A`pF!ApK%KpjP2PjpK$'`pjU<Ujp`'$6qZA6PjpU$;qU;6PjpK$;qU<Uq;$ZpjU;6PjpZ&$Pp't$;p6tPp't#Pp't#Pp'u!ApeK1'AUjUp6#;qZ;1KepZ$",
";qZ;1KepZ$;qZ;1KepZ$;qZ;1KepZ$ApjP1,AZpZ$ApjP1,AZpZ$ApjP1,AZpZ&#KpK&#;qU<Uq;$;qU<Uq;$1p`t!'jpP;-;Ppj'!'jpP;-;Ppj'!'jpP;-",
";Ppj'&!Zp,&$;qU7Uq6$;qU7Uq6$;qU7Uq6$KoU&!esP%`spA$Up;%ZsqK&'ZsqZ'&Ksqe'%Psq`&PsqP%,esqe,#1J`pKI6&AKPjoe'%1J`pKI6!1J`pKI6",
"!1J`pKI6t&#ZspKAp6$Psqe,%Psqe,%Psqe,%Psqe,%Usqe'%Usqe'%Usqe'&!1p`&%PsqP&PsqP%`oj1t#;ss6$;ss6$;ss6&#FpeKI1$PsqP&PsqP&PsqP",
"$6oj'&!,FeoZA'&Kjq`6%'eoj,%AeqjZA&$FZjpjZF&$6`rjF&#;erjA&#;ere;&#FjrjF%FssqU&ZpeP1&FssqU!FssqU!FssqUt&$Fjq`K'Ap6%6`rjK'&",
"!6`rjK'&!6`rjK'&!6`rjK'&!;erjF'&!;erjF'&!;erjF'&#`oj,t6`r`6&#6`r`6%FpAt%6ZsoZ1&6ZsoZ1&6ZsoZ1&%`spF%;ere6&#;ere6&#;ere6t&",
"#'t,1't&#'2t%2t$'16't!'16't!'2't!'16'&!FssqU&)&$FssqU!FssqU!FssqUu,6't$'16,t!'16,t!'16,t!'16,t!'16,t!'16,t!'16,&%'2,t%-t",
"$-&#3t&$,2,t!,2,t!,2,t#'K`jrF&!'2't!'2't!'2'xxxxwu&!',&1'u$16'$7&1@>&!'AP[PF'&#6@=6&#;K[K;&%';,$16'&,KF&L,t!;L,%'M'$AL1&",
"FV;t$6@=6&#,'t&%'LA%;L6$,LA&1FP[PF,&!6@=6$,L1&!1L,&$6L6t&!AL'&%-&$'LF%FL't',&$KfK&$-&#6iieU$iieP&%1fUt$(&$'hZF6't&ZeZ!6f",
";&!;f6t#;pK$Uoj6&!Uf;#'AZ&!`pj;!'oj&Usqj&;jsojA&!`sq`&1`so`1&!KqK#Zo1&,p1$6p't1pU&!6joZ'!KpK&6p`t%`sq`&FjpZ'Pif1&$Zoj,%'",
"Zoj6!1joe,%PsqK&`sq`$6pA&!Ap6&#;r;t%;pK&%PpP&$ApP#UpA&$,Zjq`A&KpK&6`jpj`6%6ssoZ$ssoU&%`p1t;Pep`P;&,sp`'t%eoe!6pA&!Ap6t#Z",
"pj#'qP&!,joj1Fjo`&;ojKjoKZoU&Pih%;qeVjq6&ZigZ%'jpjUKZpj'%'joPeoUKoj&#`oZ6';`oZt'eo`'&#6jo`PpK&'eoj,t%ZigZ%PsZUsp6&#Kp;&!",
"'ZpFjoe,%Kq`L`qF%ZigZ$6pA&!Ap6&!;p[p;t#,joZ&%'r'&$ApVpA&$AsqZ'$KpK%UsqP$1OM`pU$p`OMA&$;pPtFsqF%'Xjqe,t$eoe!6pA&!Ap6t#FpZ",
"$`p;&#;sjF&PoA!1epj6t&%ep6%Ap`t&%UpF'$;pU%;oU!'ZqK&#,es`'tZoj,&%6jqK&!KpAu&$1qZ;PoepO,&!6pK&$'Zqe,&joj,%1pjt&#6pA&!Ap6&;",
"pZ(Zp;t'`oe't`p`tArA&$1qeK;ZqP$KpK$FqZB`qAt1joe'$pKt&#eoj'&%FpjF7FjpK&%;Zp`t$eoe!6pA&!Ap6t$;A'$'A6&#1UqjA'&!1;,#';A't&%'",
"VK&!jp't&#'pe&!ep'$,;1$6A,&%'FUZUAt!1V6t!6VF&!'VKu&%KpK$;pjt#PUKt'PV,&,pP&!AL't&!6pA&!Ap6%'PUP'$KUP'&%;V1t!'<'t!;VA&%ZpF",
"%'jp1#FpF#,pj,%6pj'&$1joe,%pKt&!FpF&%6p`'%,ep6&%'jp't#eoe!6pA&!Ap6u&#1Zsj,v&Kp1t&#1pP&!Pp1wt&!Up1$'jojut&'pPu&!6pA&!Ap6v",
"u&$joj'&PpA#FpF#ApP&!`p6&#,joj,&pKt&'joetFp1&!1pKtPp6t#eoe!6pA&!Ap6u&#FpZ61p`u$'AP[PF't%Fp6&!;KZUF1&;p;&!;p;w&;FVF;&`oj&",
"Zojut&!ZojA'&%,L1&!1L,!6pA&!Ap6vu&#,pU&!6pK#FpF#Pp6&!FpA&!,eoj1&!pKt&Pp;tZoe&$eoZt6pFt#eoe!6pA&!Ap6t!AOL'&#FU1$FpF%,L1!,",
"AVK1&!;jsojA&1KZnq6%1`so`'$;pZTPZp;#AOL'&#AOL'&!,L1!,AVK1%,L1!,AVK1&#,L1!,AVK1&!1`so`1$`oe&Uoj&#;L#1FZUF'$;L#1FZUF'%Frj`",
"UF1%6pA&!Ap6!6pA&!Ap6#FK,$1L1$'KF$OOKF$OOKF$OOKF%6pF&#-'#ApA#17&#;pF&'eoj1&#pPt%,pZt!joU&$UptKp;t#eoe!6pA&!Ap6t!esp't%Zp",
",$6pF;soF%;qeVjq6$6jss6$,jpjQjpe,#;ssp;#esp'&#esp'&!6pF;soF$6pF;soF&!6pF;soF%,jqfqj,#`oe&Uoj&#Up'Zse1#Up'Zse1%AespU'#6pA",
"&!Ap6!6pA&!Ap6#joA$KpK$6oe#'ssp#'ssp#'ssp#,6ApP9,&#ApAt#Pp1&UpeK6&!pe]UF;&$Up6;F6'&#joZ&$Zojtjp't#eoe!6pA&!Ap6t!`mq't%,j",
"oU$6pFeoZUr6$ep6%Ap`#6pjP6/Kp6$UpA'#'ApZ#;pPED,#`mq'&#`mq'&!6pFeoZUr6#6pFeoZUr6&6pFeoZUr6$UpP1#1PpU#`oe&Uoj&#Up;ojU`qe'!",
"Up;ojU`qe'&;FU`jqU#6pA&!Ap6!6pA&!Ap6#ZoK$ZpZ$FoZ#'nmq#'nmq#'nmq#AsrU&#ApAt#eoj&!UsZ,%sr`1&1pejrZ1&Zoe&$jo`&$1Pp`t$eoe!6p",
"A&!Ap6t&!Pp'&%;Peo`FAp1#6peP,#'Kpe#'VK&!jp'!KpA&!Fp6#,p`&!eoj#;p6t&#Pp't#Pp'&!6peP,#'Kpe#6peP,#'Kpe&6peP,#'Kpe#,pj&!jp,!",
"`oe&UpN6#Up`A'#,ZpF!Up`A'#,ZpFt;ep,!6pA&!Ap6!6pA&!Ap6#PoZ$h$PoKt!KpZt!KpZt!KpZ#;U`peXF&#;p;t!FpP&!AQeqj'$_Zjr,%UqeVepj,%",
"FpA&!ApP&FPeqe,t$eoe!6pA&!Ap6t&!Pp'&$PsoZjoU#6q,%'q't!Kp1!`oj'&!Kp6#6pF&!Kp6!6pFt&#Pp't#Pp'&!6q,%'q'!6q,%'q'%6q,%'q'!6pP",
"&!Pp6!`oe&UsoK#UpU&;pU!UpU&;pUt!Fp;!6pA&!Ap6!1pK&!Kp1#Aoe#'o[o'#ZoAt;pe't;pe't;pe'%6pFt#;p;t6pe't6ZpPt;ZpZ$,pjA%Fpe,$,jo",
"j;%;jp1&`re1t%eoe!6pA&!Ap6t&!Pp'&#Pq`A6Fjq,!6p`&!Up6t!Fp6!joj&#jp6#Ap1&!1pF!,pZ&!'2'&#Pp't#Pp'&!6p`&!Up6!6p`&!Up6%6p`&!U",
"p6!Ap6&!6pA!`oe&UpmF#UpA&'jo`!UpA&'jo`!1L1&!6pA!6pA&!Ap6!'pe&!ep$6p#6oQo6#eo1&%1joj,t1joj,t1joj,&6pFt#;p;&%'eoj1t#,qt!'j",
"p,#KpF&!FpA%6q`Q`qZ&!`ojK1t&!eoe!6pA&!Ap6t&!Pp'&!1pe1%,jp;!6pK&!Ap6$1KZnq6!Zp;&Aq6#FpZYUZpF#ep'&eoj'&#Pp't#Pp'&!6pK&!Ap6",
"!6pK&!Ap6%6pK&!Ap6!Fp1&!1pF!`oe&Uoj&#Up'&!`oe!Up'&!`oe!6pZ&!Up6!6pA&!Ap6#ep1%1p`$'p,!AoAFoA!'p'&$'ep6t'ep6t'ep6%,;FpP>1&",
"#6p6&$'`pAt%`p,t!Up6#Zoj&$joU&;erkp6&!`oet&$eoe!6pA&!Ap6t&!Pp'&!FpK&!KpK!6pF&!Ap6#6jss6!ApeK1'AUjUp6#FsspF#;peK,';UpP&$P",
"p't#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6!Fp1&!1pF!`oe&Uoj&#Up'&!FKF!Up'&!FKF!'jpP;-;Ppj'!6pA&!Ap6#;pjU7UjpA%jo6!Po7oP!1oe&%Zp",
"At!ZpAt!ZpA&AsrU&#6p6&#'ZpPt&Fp6t!;pA#joZ&$Zoj&#,AKA;pZ&#`oet&$eoe!6pA&!Ap6t&!Pp'&!Up1&!1pZ!6pF&!Ap6!6pjP6/Kp6#ZspKAp6#F",
"pKJI1$PsqZ'&$Pp't#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6!Fp1&!1pF!`oe&Uoj&#Up'tUp't!Ass6#6pA&!Ap6$PsqF&ZoA!`o',o`!;oZ&$PpPt!PpP",
"t!PpP&!6UZp`XA&#,L,&#ZpUt&!Ap6t!6pF#pU&$Upt!`p1&#FKF&#17&$eoe!6pA&!Ap6t&!Pp'&!eoe&$eoe!6pF&!Ap6!KpA&!Fp6$Fjq`K'Ap6#Fp,t%",
";esKtPp't#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6!Fp1&!1pF!`oe&Uoj&#Up'tUp't#1ZsoZ1$6pA&!Ap6%;esK&!PoP!k#k!FoK&#ApZt!ApZt!ApZ&%6",
"pFu#UpU&#`oj'&!Pp1!Up,&!Fp;#`o`&$`oZtApPt&$Kp;&#joZ!1pK&!Kp1t&!Pp'&!Up,&!,pP!6pF&!Ap6!`oj'&!Kp6&,6'$KoZ'#;p;t&!'6joFt!Pp",
"'t#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6!;pA&!Ap;!`oj&Zoj&#Up'tUp't&Uo;,&!1pP&!Pp1&!'6joF&#FoZ,oZ#`o,Uo;&!1pe't1pe't1pe'&%,pP&",
"!1pPt!PpZ&$Up;&!jp'!FpK&!`p1#Kp1&!1pK&%'joj,t&$;pU&!1pK!'pe&!ep't&!Pp'&!FpK&!KpA!6pF&!Ap6!joj&#jp6tFoe'$1pZ&!'2'&#ZoPt#P",
"p't#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6!1p`&!`p1!Up,%joj&#Up'tUp't&jo`K'&'pe&!jp'&#ZoP&$6oe;oK#Po;`o1&,joj,t,joj,t,joj,t!pj&",
"!KpA#'B'%KpZ'&$FpZ&;p`#6pe&1pj$6pZ'%'`p6&%KpK&%1F6&#,pj'&Zp;#ep1%1pet&#Pp'&!1pe'%'ep,!6pF&!Ap6!Zp;&Aq6tjoF%'jp'%'`p,&!1p",
"1t#Pp't#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6!'jp,%,pj#KpF$6pj&#Up'tUp't&1;Zoj,&ep6%6pe&#6p1&$,pFoA#FoFjo&'`p6t'`p6t'`p6t#`p;&",
"ep1#epj$Fp`'tepeK1AZq6$ZpjP6;ZqA%PpeA2AepP&%,pj'&$AqF&#KqU;6PjpU$;pjA16Uq;t&#Pp'&#PqU61AepK#6pF&!Ap6!ApeK1'AUjUp6&%'pU&A",
"pjP1,AZpZ&#;pAt#Pp't#Pp'&!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6#;qU<Uq;#1pjP1FkpJ,!Up'tUp't&#,pF&;qU7Uq6&#Ap;&%joZo1#;oUoe&UpAt!UpA",
"t!UpAt$6qZA6PjpU#,r,!;qKJHA#1jsqK%,esqU&!PsqPtZpA&%UqU&$Zsq`,%UsqUt#1J`pKI6$PsqK$6pF&!Ap6#ZspKAp6tPpeZP$Usqe'&$ep`ZA$1J`",
"pKI6!1J`pKI6!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6$PsqP%Us`Usp6!Up'tUp't%;KPep,&!PsqP&%ep`Z;&!`ojo'#,ojoU%6peJI,!6peJI,!6peJI,&Ksqe",
"'$UpU#Assp`$'Pjr`1&!'Kjre;&$FZjpjUAt6p`t6joj6&%AerjF&#AereAt$FssqU%FZjpjUA%6pF&!Ap6$Fjq`K'Ap6t'K`p`%;erjF'&%1PepK$FssqU!",
"FssqU!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6%6`r`6&!Pqe,Usp6!Up'tUp't%KpjU;&$;ere6t1PepK&!Ppe%jpK%6ssp6!6ssp6!6ssp6&!6`rjF&!($Assp`&",
"!,61t#'61't$2t$ep6t#,t$'16't!'2't&FssqU&#2&#6pF&!Ap6&,6'u)&!'16,t&)$FssqU!FssqU!6pF&!Ap6!6pF&!Ap6%6pF&!Ap6&#-t'1,t#Up'tU",
"p't%)t$'2't&)&!FpU%`p;%6ssp6!6ssp6!6ssp6&$'16'xxxxt&!1fAu!Af1%,KVA,!1L,!,L1!';UZK1&#1KV;'!1L,!,L;&$6L,#1Q1&$-&$,L6&!6L,&",
"!'AUA'&%',t&#'fK!Af't!Ff,&,@>'$'L6!,AVK1&%KeF%PeA&!,Zg;t$1fZ$Fiig;#;ZU6&$`g&#1ie`UK,%1ieUF1&iifK!,iifU!1fF&!Ff1!ZiiZ!1fA",
"&#Pf6!'fKt!Zg6%6gZ!6gA&6f6!1if`PA&1ifUF1&;iih;$6pFu!Fp6$Aso;Fp1!6pF;soF&Fso;Fp6!'joe&$`oj'!'jpj'%;`r`;&1pF&!Fp1&!FqF&!1P",
"`peU;t%,pP!Kojt#Kp1&;sr6$1pK;soF&$Zo;%eo6&;jr;t$`p6$KssqA!1r,&!,r1&!1srjF$6srU%sspP!,sspZ!1pF&!Fp1!esse!6pF&!1p`#,pPt!eo",
"joK%Poeoe!6q`&;p6!6sse6$1ssP%;ssr;$6pFu!Fp6#6r[ojKp1!6pFeoZUr6$6qjUZoeFp6#Pp6&!1pP#,r,$ZsqZ%1pF&!Fp1&!PqP&1jspKt$,pP!Po`",
"&#1>UpF<$;ih1$1pKeoZUjq6&#jo,$'p'%Kq`p;t#ApU%KpPOL`p;!FrF&!AokoF&!1pZMZr;#6pZMUjpZ'#peON;!,p`ONA!1pF&!Fp1!FMZpZMF!6pF&!U",
"p6#,pPt!eoUoe%eoPoe!6r,%;p6!6pZN`qj,#1pZMPeqP$1OZpZO1$6pFu!Fp6#`pK'#,Ujp1!6peU,#'Ape$epA'#,Uep6#1pU&!Kp6$FfF$PpjU;AZqK$1",
"pF&!Fp1&!1ZjZ,%1jp`F;ZqKt#,pP!UoU&#Ksst&#1peU,#'Kp`&!,oj%1oe$'`pj6;p;t!'joj'%Kp,&#eoj'!,r,&!Uo[oZ&!1pA%'Fpj#6pF%'UpK#pUt",
"!,pFt!1pF&!Fp1&ApA&6pF&1pZ$,pPt!eoKjo,#1o`Koe!6pjoK%;p6!6pF&1UpZ#1pF&6ep;&$;p;&%6pFu!Fp6!'pj&,q1!6q,&Up,!,pP&,q6$`p'&eoe",
"t'q,%6q'#1pF&!Fp1t&UpA%6pet#,pP!ZoK&#Fhjpjft&#1q,%'q'&;o`%AoU$6p`,!;p;t!UpA&Kp,&!6pP$1Q1&#joLoj&!1pA&!ep,!6pF&'eo`#pUt!,",
"pFt!1pF&!Fp1&ApA&6pF&Zp6$,pPt!eoQoF#KoFPoe!6pUoe%;p6!6pF&!'q,!1pF&!ApP&$;p;&%6pF!';UZK1&#1KZU;'!Fp6!6pP&!`p1!6p`&!Ap6!6p",
"A&!`p6$ApF%6pKt6pZ&!`p6#1pF&!Fp1t&eoj'&`p,&1KZP6!,pP!AK1t#Kp1t&%1p`&!Up1&FoP%PoK$6oK$;p;t6p`&!Kp,&!Up1t$1p<p1&1pA&!Fp;!6",
"pF&!Kp'!pUt!,pFt!1pF&!Fp1&ApA&6pF%1pZ%,pPt!eoU;oZ#`o1Poe!6p;p1$;p6!6pF&#Up;!1pF&!'p`&$;p;&%6pF;soF&Fso;Fp6!;p;&!Kp1!6pK&",
"!;pA!Ap;&!Kp6$'jo`%Pp,&($ApA&!6L1#1pF&!Fp1&#)%,pU&!Fp;%AsZ'pPt&!Kp1%AOL'&!1pK&!Fp6#KQeo`SeoZQ!16%;p;tep1&!'-&!'p`t%Fp(pF",
"&1pA&!Fp,!6pF&!Ap;!pUt!,pFt!1pF&!Fp1&ApA&6pF%Zp1%,pPt!eoU'p',oe!Uoe!6p1`oP$;p6!6pF&#ApF!1pF&#ep&$;p;&%6pFeoZUr6$6rUZoeFp",
"6!;p;&!Fp1!6pF&!;pA!Ap;&!Fp6%Pp1$joZ&6p6#6pKt#1pF&!Fp1&#eo`%6pF&!Pp'$,rUeo;pP&%;ZjeK6!Kp1%esp'&!1pK&!Fp6#jssqj&#;p;&%KpK",
"t&!FpAt%Zo`#`oZ&1pA&!eo`#6pF&!;p;!pUt!,pFt!1pF&!Fp1&ApA&6pF$6pU&,pPt!eoZ!Zo;AoK!Zoe!6p6Foj$;p6!6pF&#Up;!1pF&!,pZ&$;p;&%6",
"peU,#'Ape$epA'#,Uep6!;p;&!Fp1!6pF&!;pA!Ap;&!Fp6%1pP#;pA&6p6#'jo`t#1pF&!Fp1&#eo`%6pF&'joU%UpU,#;`pP&$PsoAFp1%`mq'&!1pK&!F",
"p6#<ApA=Foj=&#;p;&$,pe't&!eoj't%joP#Pp'%1pA%,KpA#6pF&!;p;!pUt!,pFt!1pF&!Fp1&ApA&6pF$`p1&,pPt!eoZ!AoPZo6!Zoe!6p6,p6#;p6!6",
"pF&!'jp,!1pF&!FpA&$;p;&%6q,&Up,!,pU&,q6!;p;&!Fp1!6pF&!;pA!Ap;&!Fp6&`oj'!Uoj'&6p6$Pp;t!1pF&!Fp1&!'jo`%6pF%1Uoj6%ep,%UpP&#",
";qeLjeKp1&%Pp'&!1pK&!Fp6%1oe%;o`t;p;&$Zp;%QA&%6pPt%6p;#;p6%1pZRZpe6$6pF&!;p;!peYW;#,pP54#1pZOKZp1&ApA&6pZGKpU&!,pPt!eoZ!",
",ojoj#Zoe!6p;!ZoU#;p6!6pF&'Pp`#1pF%,Fjp,&$;p;&%6p`&!Ap6!6pA&!`p6!6pK&!Zp1!6pF&!;pA!Ap;&!Fp6&Ap;'pP&!;p;$,joe't1pF&!Fp1%A",
"Zq`%6pF!1U`pZ,&pe&;pP&#epA$'Kjp1&%Pp'&!1pK&!Fp6%AoZ%FoPt;p;&#ApZ&pU&%Up1t%Kp'#'pK%1sqe,%6pF&!;p;!ssoK#,ssp#1ssp1&ApA&6sq",
"1&!,pPt!eoZ#`pP#Zoe!6p;!Aoj'!;p6!6pUCFUr1#1pe]`rK&%;p;&%6pK&!;pA!Ap;&!Kp6!,pe&'q1!6pP&!;p;!;p;&!Pp6&'joZ;p6&!;p;#AKepZO&",
"1pU&!Up1$Ps`%6pF!6rK,%,pP&,pP&!'pj&,q1&%Pp'&!1pK&!Fp6%PoK%UoAt;p;&!'joj,&pU&$,p`t&`oe%eo`%1pebeqP$6pF&!;p;!pj_]A#,ssp#1p",
"nlp1&ApA&6pjfjpF&!,pPt!eoZ#Fp;#`oe!6p;!'pA!;p6!6ssj;$1sre6t;p;&%6pF&!;pA!Ap;&!Fp6#epA$'Kq1!6pj&!Kp1!1pF&!jp6&!PpZoe&#;p;",
"#Zss'%1pj&'jp1#FqU;(%6pF!1Z`qj6$,pP&,pP&!1pU&!`p1&%Pp'&!1pK&!Fp6%`o;%eo1t;p;&!PpF&!pU&$FpF&!'C%'pP%Pp'$1pA%6UpP#6pF&!;p;",
"!pUt!,pU:9#1pF&!Fp1&ApA&6pF#,pj'&,pPt!eo`#'2$`oe!6p;#UoZ!;p6!6speZK'%1peWjp,t!;p;&%6pF&!;pA!Ap;&!Fp6#;qjKPkKp1!6q6%'ep'!",
"'pe'%6q6&!1rK&#ApA#Ufjpi'%1q6%;q1#jp,&$6pF%,Kjoj'#,pP&,pP&!6pA&!Kp1&%Pp'&!1pK&!Fp6!8joA9p71&$;p;&!joe&#pU&$eoj'&!;pe%6pA",
"*Ap6$1pA&!Up;!6pF&!;p;!pUt!,pKt!1pF&!Fp1&ApA&6pF$PpK&,pPt!eo`&$`oe!6p;#;p'6p6!6pFt#1pF$KpAt!;p;&%6pF&!;pA!Ap;&!Fp6$KsoAF",
"p1!6pPjU61KepP$PpeK16UjPp6&#`q,&#ApA&Kp1&%1pZoP16UoUp1!1pZ&%6pF&!Fp;#,pP&,pP&!6pA&!Kp1&%Pp'&!1pK&!Fp6!jssqe&$;p;&!joe:7p",
"U&#6pU&#KpP%KssK$1pA&!6pP!6pF&!;p;!pUt!,pKt!1pF&!Fp1&ApA&6pF$,pj'%,pPt!eo`&$`oe!6p;$joF6p6!6pFt#1pF$,pet!;p;&%6pF&!;pA!A",
"p;&!Fp6%1P[F1!Fp1!6pAPsoe'$'esoPAp6&#ApZ&$ApA&Kp,&%1pFjreFp1!Fp;&%6pF&!,pU#,pP&,pP&!6pA&!Kp1&%Pp'&!1pK&!Fp6!UZ`oj^eoe[P&",
"$;p;&!jssoU&#Up6&#ZpA%`ss`$1pA&#eo`!6pF&!Ap;!pUt!,pKt!1pF&!Fp1&ApA&6pF%PpK%,pPt!eo`&$`oe!6p;$Po`6p6!6pFt#1pF%Pp;t;p;&%6p",
"P&!;p;!;p;&!Pp6t!Fp1!6pF'KeqU'&'UqeK'Fp6&#ApA&$FpF&eojt1pFAqj;Ap1!ApA&%6pF&#`oj#'pU&1pP&!6pK&!Up1&%Pp'&!1pK&!Fp1$FoP%PoK",
"t!;p;&!jssoU&!,pe&$ep,$'pU51Up'#1pA&!'p`!6pF&!Kp'!pUt!,pKt!1pF&!Fp1&ApA&6pF%'joj'$,pPt!eo`&$`oe!6p;$6p;p6!6pFt#1pF%,p`t;",
"p;&%6pj&!Kp1!1pK&!jp6t!Up,!6pF$,1t!1,$Fp6&#`oj'&$FpF%ApUt1pF!'61'&!1p`&%6pF&!'joj$joj'%KpP&!,pe&'jp1&%Pp't&!`p'$UoA%`o;t",
"!;p;&!55pU&!FpF&#'pj%;pA&!;p;#1pA&!FpP!6pF&!`oe#pUt!,pKt!1pF&!Fp1&ApA&6pF&KpK$,pPt!eo`&$`oe!6p;%eoPp6!6pFt#1pF&Up6&%;p;&",
"%6q6%'ep'!'pe'%6q6t'pj#6pFu!Fp6&!1pP&%FpF$1pj,t1pFt#jp1&$6pF&!FpU$`p6%`pP&#jp1%;q1&%Pp't&1pe%eo6%jo,t!;p;t&!pU&!ep'&#6pU",
"%Pp,&!'pP#1pA&1epA!6pF&PpP#pUt!,pKt!1pF&!Fp1&ApA&6pF&'joj'#,pPt!eo`&$`oe!6p;%Kojp6!6pFt#1pF&6pZ&%;p;&%6pPjU61KepP$PpeK16",
"UjPp6%*,A`pP#6pFu!Fp6&!Kp6&%KpK#6joj6t!1pFt#Kq`F1.#6pF&;epA$ApjP1;`UpP&#FpjP16UjUp1&%Pp't!(1KjpF$'p'$,oj&!,JUpUI't!pU&6p",
"U&$FpF%eoe&$eoe#1pUCKeqZ#6pUCPep`'#p`JI6!,pKt!1pF&!Fp1!AHUpUHA!6pF&!KpK#,p`JI1!eo`&$`oe!6p;%1r6!6pFt#1pF&!`p1&$;p;&%6pFP",
"soe'$'esoPFp6$1sqe'#6pFu!Fp6&!joetKpK#ZpPEE#1pFt$Ksq'!6pF!,FPjqZ'%`sj1pP&#'esoLp1$1J`pKI6&!`sZ%1oe%;o`&!6ssq1t!pU&Up6&$P",
"p6$,pP&$Pp,!1ss`,#6srZ'$sspP!,pKt!1pF&!Fp1!esse!6pF&!'joj'!,sspK!eo`&$`oe!6p;&`q6!6pFt#1pF&!;pU&$;p;&%6pF'PeqU'&'UqeP'Fp",
"6$1spU,$6pFu!Fp6&;pFtPpP#Zssq'!1pFt%A`so'!6pF!6sU&!'UqZ;,pP&$'UqeP'Kp1$FssqU&!`qjF&AoU%KoP&!6ssq1t!pU%,pe&%Pa%ApA&$;pA!1",
"speUA%6sjZK;'%sspP!,pKt!1pF&!Fp1!esse!6pF&#KpK!,sspK!eo`&$`oe!6p;&Fq6!6pFt#1pF&#ep,&#;p;t&!,1t!1,t'E6'&'B1u!1B'&6B't6B6#",
"Zssq'!'B1t&#1D'!6pF!6ojZK;&%'1't&#1,tFssqU&!;A;1xxxxx%;f;&$;f;!;f,$AfK%f;!1f`&$Uf1!Uf,&$'fU!6iif,&$6U`h`!1fAt!'fKt#'ig6t",
"$.&$Uf,&$'fU$'Uf1&#'fF$'Uf1&#'fF#Fie&!'fF&!Ziie'$1ieUF1&6f;t%1ieUF1&#1fF&!Ff1&1fAt!'fK$1f6%'ig6#6f1#'fKt!PifPt%'fKt!'ig6",
"&$;iih;&!.t!6U`i6&$(&$,pP&$Pp,!1p6$KpZ$,p1#UpA&!6pU#6pP&$Kp6!6ssp,&#Fspe!6pFt!'pPt#,sq;t$eoe&$6pP&$Kp6#,`o`o6&#Uoe'#,`o`",
"o6&#Uoe'#;RUoj&!Uoe'&'ssp,$6srU%6pAt%6srU&!1pF&!Fp1&6pFt!,pP$;p,%,sq;#;p'#,pPt!ZspUt%,pPt!,sq;&$;ssr;&!eoetUsq6%'AUepeUA",
"'&eoe&$eoe#,p;$Uoj`$1p,#,pe&!`p,$`p'&!'jo`#1ONep,&!,qZO!6pFt!'pPt#'OZp;t$eoe&%`p'&!'jo`$Ao;Ko6&!;p1$Ao;Ko6&!;p1&!,a,&;p1",
"&!1pZKpeO'$6pZMUjpZ'#6pAt%6pZMUjpZ'%1pF&!Fp1$<KpU>1&!,pP$Ap&'OZp;#Foj$,pPt!AOpUt%,pPt!'OZp;&$1OZpZO1&!eoe&%AqZO,$'`oeUBU",
"eo`'%Pp,&!,pP$pF$`j`j$;p%PpA%;pK%;pK&!Fp;t!,pZ&#ApF&#6pFt!'pPt&$Ap;t$eoe&%;pK&!Fp;$;6!Ko6&,joF%;6!Ko6&,joF&!,j`'&,joF&#A",
"p1!joU&%6pF%'UpK#6pAt%6pF%'UpK%1pF&!Fp1$ssK&!,pP$Foet!Ap;#Ko`$,pPt&#pUt%,pPt&#Ap;t%;p;t#eoe&%`p1&%'`o;&!;o`'$Ap;&!;p;$eo",
"K$jeZo'#Foe%'joj'$`oj'&`oj'&eo`t#Zp,&#Kp,&#6pFt!'pPt&$Ap;t$eoet`oj'&eo`&#Ko6&Zo`&$Ko6&Zo`&#6pZ6%Zo`&$Pp#joU&%6pF&'eo`#6p",
"UD6'&$6pF&'eo`#ZsssoU!fjpjhF&!,pP$PoZt!Ap;#PoU$,pPt&#pUt%,pPt&#Ap;t%;p;t#eoe&%p`tZo;%'$;oZ$,pP&!Pp,$ZoU#'oZUo1#KoZ&FpF#A",
"pF&!;pF%Ap;t!;pK&$Kp,&#6pF!,AVK1%'pP&#AL,&#Ap;&$6NjojOK6$;pF%Ap;&#Ko6%Foj,&$Ko6%Foj,&#'1Fje$Foj,&$`o`#joU&%6pF&!Kp'!6sre",
"K&!6pF&!Kp'!ZsssoU$6pFt!,pP$;K;t!Ap;#;K6$,pPt&#pUt%,pP$'At#Ap;$A'&%;p;&#6NjojOK6!'pU&%'oP#1PjoU6#Po'$eoe&!eoe%UoZ#1oPKo;",
"#UoP&'eoj'!eoe&$eoj'$eoet#eoj'&$Kp,&#6pF;soF$'pP&!6p`&$Ap;&$PssqP%eoj'$eoe&$Ko6$,pA&%Ko6$,joA&!,'$Ko6!,joA&$'pP#joU&%6pF",
"&!Ap;!6ssoU&6pF&!Ap;!16ApP:6PpA61$6pFt!,pPt&#Ap;&$,pPt&#pUt%,pP#;jUt#Ap;!,Zo'&%;p;&#PssqP!'pU&%6oA#eoZUp,!Ao6$Pp,%'pP%Ko",
"e#;oFAoF#`oF&!ApFAp;&$ApA#;pAt!FpA%'9Up;:1!6pFeoZUr6#'pP&'joj,&$Ap;&$KmqnjP%ApA#;pA&$Ko6$`oZtKo6$`oZ&#f$Uo,!`oZ&%1pA#joU",
"&%6pF&!;p;!6pA%1KepP%6pF&!;p;$1pF&!Fp1&6pF!6PfZ;%,pPt&#Ap;&$,pPt&#pUt%,pP'UpPt#Ap;Ajoj'&%;p;&#KmqnjP!'pU&%Ko,!6oU#AoK!,o",
"K$;pA%;p;%Ap#Fo;6oK#eoA&#`ojeoZteoe#`oet!,jo`&;ssqe!6peP,#'Kpe#'pP&UpF&%Ap;t$eoet#eoe#`oe&'WeoZVPoj,&!'WeoZVZoj,&#FoeP`o",
"U!Koj,&%Ap6#joU&%6pF&!;p;!6pA&!,pj%6pF&!;p;$1pF&!Fp1&6pGsoU$,pPt&#Ap;&$,pPt&#pUt%,p`eojAt$ApepU't;p;t#eoe&$'pU&%Uo#PoA#'",
";6#oU$'pP%Kp'%6p'!Po1,oU#p6&#6r1tAp;6pAt!Up1&;ssqe!6q,%'q'!'pP%;pZtAp;t$eoet#Ap;6pA&'ifjp;&#'ifjp;&%;UjZA'1p;tPp'#joeS1#",
"1;FpP<1$;p;!6pA&#Zp1!1;FpP<1$;p;$1pZOKZp1&6pKjeKF`qA#,pPt&#Ap;&$,pP&6UP1&$pU&'FZA&,rZ,t%Aqj;t#;p;t#eoe&$'pU&%Uo#Po;&#oU%",
"`oe%`o`&,p1!Zo(o`!,p,&$ZpUt!'jo`Zoj't6pP&!'4Up;51!6p`&!Up6!'pP$'joj,tAp;t$eoet#'jo`Zoj't#'eoUt&'eoUt&'eoUt!`oe$jso;#ZspU",
"$;p;!6pA&#ApF!ZspU$;p;$1ssp1&6pjF%;pj#,pPt&#Ap;&$,pP%'qj&$pU&PqA%;pjFt&'ZpZ,&%*ApA*&$eoe&$'pU&%Uo#Po;&#oU%Kp,#'pK&'p6!ej",
"#k!1p&%`pe't!FrFt!`oj't!Kp,&#6pK&!Ap6!'pP$UpAt!Ap;t$eoe&$'>ZrZ>'&$Poe't&Poe't&Poe't!pU$jojh;#UkqkU$;p;!6pA&#Pp;!UkqkU$;p",
";$1pnlp1&6q'&jp,!,pPt&#Ap;&$,pP%Ar;&#pU&jqZ#'PqPt&;jq;tZssU&$eoe&$'pU&%Uo#Po;&#oU%;pA#6p6&!joA'o`#`o';oe&$ArFt!'jpj'tFpA",
"t#Kp,&#6pF&!Ap6!'peVZpZt#Ap;t$eoe&$1ssr1&#6p6#6ZKt6p6!;U`F,&%6p6#6ZK&!1pF$joU&%6pF&!;p;!6pA&!'jp,$6pF&!;p;$1pF&!Fp1&6pZ&",
"!Pp6!,pPt&#Ap;&$,pP%1r'&#pU&ZqK!1erPt%Upjp;tZssU%1CjojE,#'pU&%Po#FoF#1`K#oP%'pP#Kp'&!`oK,oU#Uo1FoZ&#'joZ`oj't!KpKt'jo`t$",
"Kp,&#6pF&!Ap6!'sq;t#Ap;t$eoe&$1njpnj1&!'eoP#'ej,&%'eoP!UoeZp,&#'eoP#'ej,&!ApK=joU&%6pF&!;p;!6pA%';ZpZ%6pF&!;p;$1pF&!Fp1&",
"6pF&!Ap6!,pPt&#Ap;&$,pP&FfA&$pU&1ZjU,!Ao`6pPt%ojBp;t/FpF/%Fsso6#'pU&%Ao6!'oe6,Zo;!6oA&`oe#`o`&#UoP6oP#Ko6KoU&#Kp6;pPt!;p",
";tPp6t$Kp,&#6pF&!Ap6!'peWpe't!Ap;t$eoet%;p;t#Uoe'#UoAtUoe',oU#6oK&#Uoe'#UoA&#PsqU&%6pF&!;p;!6pecjqj,%6pF&!;p;$1pF&!Fp1&6",
"pF&!Ap6!,pPt&#Ap;&$,pPt&#pUt!AF!,pPt%Z,!Ap;t%;p;&$Abqd1#'pU&%,oK#Pr`#Ko,&Kp,'pK&#KoZAoF#FoAUoK&!,pZ#`p1t;p;&%1pUt%Kp,&#6",
"pF&!Ap6!'pP$PpKt!Ap;t$eoet%;p;t!;p1#AoU!,6'&!;p1!1K1$je&!;p1#AoU!,6'%`pmpU&%6pF&!Ap;!6ssZ,&6pF&!Ap;$1pF&!Fp1&6pF&!Ap6!,p",
"Pt&#Ap;&$,pPt&#pUt%,pPt&#Ap;t%;p;t#eoe&$'pUte`'#'AK1#'`e&!6p;6p6&#Fo`Ko;#;oKZoA&!Up1#6pZt;p;&%`oj,t%Kp,&#6pF&!Ap6!'pP$'j",
"p1tAp;t$eoe&$1ijpji1%,joK#1je'!Po1&,joK&#6oK&,joK#1je'!Po1%pZ%joU&%6pF&!Kp'!6pZSKA1&#6pF&!Kp'$1pF&!Fp1&6pF&!Ap6!,pPt&#Ap",
";&$,pPt&#pUt%,pPt&#Ap;t%;p;t#eoe&$'pUtAoZ'&!'ZoA&!'pKFoj&$;ojUo1#1oUeo6&6pU%Zp6&%;p;&$ApFt&Kp,&#6pF&!Ap6!'pP%;p`t;pAt$eo",
"e&$1ssr1%Zo`$Zo6#Po1&Zo`&#;je,&Zo`$Zo6#Po1$1pK%joU&%6pF&!`oe#6pAt%6pF&!`oe%1pF&!Fp1&6pF&!Ap6!,pPt&#;pA&$,pPt&#jo`t%,pPt&",
"#;pAt%;p;t#eoe&$'pUt!Ao`;,#,;`oA&$ZoZUoZ&$1p`o'#'oZjo,&`p,%1p`&%;p;&#'joet&!Kp,&#6pF&!Ap6!'pP&UpF&%1pZt$Zp,&#'EPpPE'$Aoj",
",$eoReo1%Aoj,&,UoZ'&Aoj,$eoReo1$Ap;%joU&%6pF&PpP#6pAt%6pF&PpP%1pF&!Fp1&6pF&!Ap6!,pPt&#1pZ&$,pPt&#`p,t$,pPt&#1pZt%;p;t#Zp",
",&#'pUt#;jpkpj;&%KojeoF&$'pk%kp'%ApK&!UpA&$;p;&#ApUJI1&!Kp,&#6pF&!Ap6!'pP&'jp1&%ep`J1&!FpeKI1&#;p;&$,joA%Zno1$,joA%'Koe;",
"&!,joA%Zno1$Pp,%jo`J'$6pUCPep`'#6pAt%6pUCPep`'%1pF&!Fp1&6pF&!Ap6!,p`JI1&$ep`J1!,p`JI1&#FpjPIA&#,p`JI1&$ep`J1&#;p;t#FpeKI",
"1!'pUt%,;Q;,t!6r1&%jp`%epj%'joj'&!,pj'&#;p;&#FsspF&!Kp,&#6pF&!Ap6!'pP&!;p`&%6sqK&#`spF&#;p;&$`oZtPo1$`oZ&KoeAFM'#`oZtPo1",
"$`oj&jsp,$6srZ'$6pAt%6srZ'&1pF&!Fp1&6pF&!Ap6!,sspK&$6sqK!,sspK&$`spe&#,sspK&$6sqK&#;p;t$`spF!'pUut#jpjtepU%Zp`%KpF&$KpK&",
"#;p;&#FsspF&!Kp,&#6pF&!Ap6!'pP&#ZpF&%,UesK&#'K`jrF&#;p;&#Koj,tPo1#Koj,&Ksp,!Koj,tPo1$pZ&jsp,$6sjZK;'%6pAt%6sjZK;'&!1pF&!",
"Fp1&6pF&!Ap6!,sspK&%,UesK!,sspK&%F`jre&#,sspK&%,UesK&#;p;t$'K`jrF!'pUxxxvu&(v'VF!'dddb'#6V6&'AP[PF'&$;KUZPF'&$;KZUF1&%;F",
"VF;&$1FP[PF,%,L1&!1L,&$-t!'AZF1#,FZP;&$-&$;UZP6#6PZU;&%'8t&#8't&%A;!;At$,L'6Q6!'AUA'$,L1!,AVK1%;L#1FZUF'$,L;&$6L,#FK,$1L",
"1$'KF$FL,&!'LF$OOKFuAOL'&!FKA&#;L6&#,Q,u61t&!'AUepeUA'%e;u$AKZUA,!Uoj,#UsssoU#,r,$;jsojA&,ZsojF&1`so`'&1`so`1&PsqK$6pA&!",
"Ap6&#;p;&%Ajr6,jrP&!ZpZ&!UsBsU&$ZpAt&#ApZt&#AeoF!FoeAt!1p;r1Kqj1#6pF;soF$Up'Zse1#'pe&$`p'#joA$KpK$6oe$FpU&!UpF#'ssp$AUtF",
"6$esp'&!jo`&!,joj'&#6p6t&%'eoFt%'`oeUBUeo`'$p`;t&%1`soepA$,ssso,#ArA#;qeVjq6$'jqeZjqA$,jpjQjpe,$,jqfqj,$Kq`L`qF#6pA&!Ap6",
"&#;p;&%eojPZoeUoePjp;%6r6%;pjUeofoeUjp;&!KpPt&%PpKt%AeqA!AqeA&%1p`KFjofFKjoP#6pFeoZUr6#Up;ojU`qe'#Up1&!,pU$ZoK$ZpZ$FoZ%Z",
"p;%6p`$'nmq#;pZ$-$Foj6#`mq'&!jo`&!`p;&$6p6t&%PpAt$'`o;&!;o`'#r`;t&!,jq`ZrU&PssrP$'r'#ep6%Ap`$UpU1#';q,#UpA'#'ApZ$UpP1#1P",
"pU$joj,%1pj#6pA&!Ap6&#;p;&$1p1#KpZ#'pZ%6r6%Zp,#ZpZ#,pZ&;p`u!`p;t!AeqZ1$;`qeA&#1pP#ApA#Poe#6peP,#'Kpe#Up`A'#,ZpF#;pK&!Fp;",
"$PoZ$h$PoK%,joj'#'eoj,t#KpZ#,epZPepeLpj6&#Pp'&!jo`&FpP&%6p6t&$6pZt%Zo;&$;oZ#so`6t%UpK,#'epP&,ssr,%,G,#'VK&!jp'#q'&PpA!,p",
"`&!eoj#,pj&!jp,!,pP&!AL'!6pA&!Ap6&#;p;&$6PK$6pF$Zoe&UpU&eoZ$FpF$Zoe%,joj,u!,joj,&$AeqU1&#;`qeA&1p;#,p,#;p1!6q,%'q'!UpU&;",
"pU#'p`&!`p'$Aoe#'o[o'#ZoA&ApP#PpFt#;pe'$,erkrj6&$Pp'&!jo`%,joe'$'1%6p6%1't!'joj,t$'oP!;qeK1#Po'!sq`6t!'pe%Fqe&!PsspPu!Kp",
"1!1pU&!1kK!6pF&!Kp6!6pP&!Pp6!'pPt!6pA&!Ap6&#;p;t%,p6$Poj&!(&!joP$6p6$Poj$'ep6u$6pe'&AeqU1t!6`qeA$1p;#,p,#;p1!6p`&!Up6!Up",
"A&'jo`$Up,%'pU%6p#6oQo6#eo1&!Zp61pZt#1joj,&6p`1!'A`pA&%Pp'&!jo`%`p6%AoeF,!1p,!,FeoAt!UpAt%6oA!;oZKUk#Ao6!ssZ6&%1pK$1p`p,",
"&,ssp,u!Fp6!;pAt!Ap1&!1pF!Ap6&!6pA#ZojA'&%6pA&!Ap6#:FpF:&%,p6$Pojt%joP$6p6$Poj$UpFu&FpU$1eqU1t&6`qe1!1p;#,p,#;p1!6pK&!Ap",
"6!Up'&!`oe$;pF%Ap;%'p,!AoAFoA!'p'&!,joaoj,t!'ep6&!Fo`'%'joK&%Pp'&!jo`$FpP&ZrU1p1UrZt;pZt&Ko,!;oK#Fo;!,oK!sspZ6&#;p1#'eo`",
"1p;&!PssPt%1KZnq6!;p;t!FpZYUZpF!Fp1&!1pF#Frj`UF1%6pA&!Ap6!'ssr'%6KU[p`]eoj!++(!joP$6p`Weoj#FpP,++(!++(,PpF#FojP,t&%6ZpF!",
"1p;#,p,#;p1!6pF&!Ap6!Up'&!FKF$'jo`%Zoj'&jo6!Po7oP!1oe&$;rAt#ZpA&#ZoA&!Po`&%Pp'&!jojWpe'&,FesseF,#<&'joj,t&Uo#;oK#;oK#oU!",
"ssrZ1&;p1#Poj,1p;&!,ss,t$6jss6!;p;t!FsspF!Fp1&!1pF$AespU'#6pA&!Ap6!'ssr'$Kssqj!ssp!joP$6sqj!6sssoA!Assso6!FojK't&%'AeoF!",
"1p;#,p,#;p1!6pF&!Ap6!Up't$Up,#'pU&!ZoA!`o',o`!;oZ&%`pet#PpP&#'p,&!1p'&$Pp'&!jspF&%1KZjpjZK1%6p;%UpAt&!Uo#;oP1;`o6#oU!sss",
"oZ1$;p1!6pA!1p;&#PsqPt$6pjP6/Kp6!;p;t!FpKJI1!Fp1&!1pF&;FU`jqU#6pA&!Ap6#0ApA0$;peA;ApK@;!ssp!joP$6pUOF!;sssoA!Assso;!;rP,",
"t&'Fjq;!1p;#,p,#;p1!6pF&!Ap6!Up't$;pF#Ap;&!PoP!k#k!FoK&$;r;tApZ&%jo6&!;p'&$Pp'&!jojbpj't,epe,&!Kq;#;pUt&#Uo#;sF$oU!ssspP",
"'#;p1'joZ#1p;&#,jsoj,t$KpA&!Fp6!;p;t!Fp,t!Fp1&!1pFt;ep,!6pA&!Ap6&#;p;&$`oj$,p6&#00-!joP$6p6&$PpF@@<,!,@@<FpP$,PrP,t!'Fjq",
"P,#1p;#,p,#;p1!6pF&!Ap6!Up't%jo`#Zoj&#FoZ,oZ#`o,Uo;&#'joe`oj,&$1pe'&%UoK&!Zoe&%Pp'&!jo`$FpU&%'eo[o`'&!KqA,joj't&#Uo#;oU;",
"`o'$oU!sssP,%;p1Zoj'#1p;&$KsoKt%`oj'&!Kp6!6pK&!'B6!;p;t!;pA&!Ap;t!Fp;!1pP&!Pp1&#;p;&$joU$,p6ujoP$6p6&%`oj6u&6jo`&!,PrP,&",
"#'FjqP,%1p;#,p,#;p1!6pF&!Ap6!Up't%Up,'pU&#6oe;oK#Po;`o1&#Zp7pZ&#,joj,t;oj6%6pF&%Pp'&!jo`%`p;&$Up2pU&#Kqep;t&$Po#;oK!AoF$",
"oP!ssqU,&!6pUp6$Ap6&$'jrj't%joj&#jp6!,pe&!ApF!1pZ&!'2'!1p`&!`p1!1L1&!6pA!'pe&!jp'&#;p;&$pK$,p;$'-t%joU$;p;&%,joj,u$,joj,",
"&$,PrP,$'KjqP,&!1p;#,p,#;p1!6pF&!Ap6!Up't%;pA;p6&#,pFoA#FoFjo&#ApP#PpA&'`p6t!KpjP;A`qK&%Pp'&!jo`%1pj'&!ApP#PpA&#KrUt&%Ao",
"6!;oK#k#6oA!ssoU,&%rP%Zp,&%KrK&%UkU#Zp;&Aq6#ep6&ep6!'jp'%'`p,!'jp,%,pj#6pZ&!Up6#ep6%6pe&$;p;&$eo`$ApP$`oZ&PkP&`oj$PpP$UZ",
"K%6p`'u!'`p6t!,Pr;!;jqP,&$1p;#,p,#;p1!6pF&!Ap6!Up't&joUPoj&%joZo1#;oUoe&!,joj'#'joj,%UpAt!FsspK&$Pp'&!jo`&KpU&,joj'#'joj",
",&#Fpj't&%,oK!;oK#KoA!Ko,!srU1t!Zpe'$,pjt'jpj'&$6r6!ApeK1'AUjUp6#;qZ;1KepZ$ApjP1,AZpZ$;qU<Uq;#'jpP;-;Ppj'#;qU7Uq6&$;p;&$",
"PpK'6eok;'Ap;%6r6%PpK,AjpjA,Kp6&FpUu!UpFt%,PpF!FpP,t1p;#,p,#;p1!6pF&!Ap6!Up't&PojeoP&%`ojo'#,ojoU&!Zp;%;p`$6peJI,!Aq61F[",
"F16qF!1J`pKI6!jo`&!`p;%Fp6%6pF&$Fo;u!e`',K6#,K;'`e#spU1t$UpjP7UjpAt!KpK&%;r;#ZspKAp6$Psqe,%Usqe'%PsqP%;ss6%PsqP%+ApA+$,j",
"sK;sj&;r;%,js[s`&#UpFt&%FpUt&#,PA!AP,t#1p;#,p,#;p1!6pF&!Ap6!Up't&6r6&%Ppe%jpK&FpU&!UpF#6ssp6!'`o6&$6oe,!FssqU!jo`&!1pe'%",
";K&!K;t6u#AoZ'&!'ZoA#sZ1t%6ssUt#'k't`p`%Fjq`K'Ap6%6`rjK'&!;erjF'&!6`r`6&!6ZsoZ1&!;ere6&jsspe%1epjP#Kjpe6&!`p`&!1eqZ',`pj",
"P,&#'ep6t&#6pe'ut&!1p;#,p,#;p1!6pF&!Ap6!Up't&!jpjtFpU%`p;%1pj'&!'jp1!6ssp6#'6t6,#FssqU!jo`&#KpPv&$Ao`;,#,;`oA$qZ1t&'eoZ;",
"Peqe;t%Lt#-&#,6't$'16,t!'16,t#-t#,2,t!'2'&#jsspe&!,1&!1,&%2&%,6'%'6t!,LAt&#AL,xw&#;jpkpj;%oZ1t&#Pje,$,1't&!(xxwvt#,;Q;,&",
"!6xxxxxxvt#;k;t$;Q;$6QAt#AQ6$;Q;&#,ig;$'KgtAfF&#A'&!,F$)$*#Pf;#;fP!FfF$C'!;YYV;%(&$-t&!Zf1#;fK%`f'#AfK#=%6<'$A`,&#<&!'FK",
";&;KF!iift&%'A;'$'G'$FL'&!1'%'1&!'LF#++!++!++*!+++)!'::9'!6@=6wt#;p;t#6pe'#,joe'$6UkZ;$,eoj,#'ep6&!1sqA#;eqjtepe&!Fo`'%,",
"eoF#Zp6#1jo`#Up;#;pU!FpF#1pj#FsspF#6`p`F%FjpjF%,KfK,$6pZ%Up6#ApP$ep,#FpF$6pU$f&!AjpjA$UrZ'$Uoe!ssp#Zj;#,f,#;jZ$epe$jpj$6",
"pU&`oj,#1joZ&Up6#`sq`!`sq`!PsspP!sssq!1ssr1!`sq`wt#;p;t!,joj,#'eoj1$;soK$1p`'#,joj,&!+Foe,!ApU6oj&%;oj`oA&6jo`'#,eoj1#'j",
"oe$ApF!Up6#6pU!FpF#ApZ#;YYV;#eZ-Ko,#PpVpP$a<a$Up;%;pK#ep,#;pU%joj%Up1!1pZF$'jrj'!1p`ZpZ$Uoe!ON`p!,pZ#FpF#Zp,!;r6!;r;$FpF",
"$,qF#Fq,$ApA$`sq`!`sq`!PsspP!sssq!1ssr1!ZigZwt#;p;t'`p6$Up;$'pK(Fp1$ApU$;p`&%'UoZ'#Ae6!1oj&%`oKAo`&!6jo`',eoj1%Zp,#,pZ!P",
"p6#6pP!FpF#PpKt&#'1'$jK!1p,#,p1!1o6#6o1!,pe&1pU!6pZ$Zp6%Up1$ApF!'1Ajo`#AsoA!Poe#FpP!'joKtKp!1p`#KpK#`p1!6r1!1r1%Up6$jp6#",
"6pe$1pP%6@=6!6@=6!1@@<1!@@@=wv;p;tPpF$FpK%;oe%`oF%PpF$FpP&#'`oK%,$1oj&$6p,'jo6&!6jo`eoj1&Up1#'p`!Pp6#1pP!ApA#Zp;t&$';RjP",
"!AoU%UoA!Fo'#'oF!ApF&ApK!Pp;#'pe&Up,$FpA%Fp1!AsoA!eoU$Upkp1tKp!1p`#FpF#`p1#ZpU$PkP&!PUP$,A1%1A,$KUP'xt%+ApA+%ApP$6pU&,6,",
"%PoK&Zp6$UpA&!1pU1&#1oj&$ZoU#KoZ&#6qj1&!ep'#6pP!Kp1#1pP!ApA#jp't&#,o`RjP!FoP%PoA!Fo'#'oF!Pp6&Zp1!Zp'#1pP&joj%Zp1!FKUjo`#",
"'jrj'!joU%PqjKt!Kp#eoF#1p1#Foe%-&(xv$'ssr'#6pZ$,jo`'t!'jo1&'eoj,#'`p6&1ZjpU&!1oj&#1p1#,p1&!,ep`'&1pZ$Pp;!Kp1#1pK!;pA!,pe",
"t&$Kj$'jP!6oj%jo6!Ao'#'oA!Pp6%,pe#Zp'#1pU%;pP$,pe#epeP,$KrK#<1&1A;t$6G#'1%-%1'xvt&!'ssr'#ep,$Zp1t!'`oU&#;pZ$1pe&$6jo1&1o",
"j&#PoZ%UoU&,eoko`'%Up;#,pe#Kp1#,pK!;p;!6pP$1OOL1!Kj'#FjP#`o`B`o`#'oF#Fj'!ApF%PpA#Kp;#'pe%`p1$KpF#)&#'G'xwu$@KpK@$ApU$6pZ",
"t,eoZ'&!'`p6$ZpA&%PoP&1oj&!,p;%6p,$,eoj16jo`'#,pe$KpF#Fp,#,pK!;p;!FpA$FsspF!'kZeF`P#,`rZ,$Po[oK$`oj1#'pj$'joe'#FpK#6pZ$'",
"joj'xwv%;p;&%PpK$FpP&$6joU'&#UpF$KpP$PZ;%Ao`&1oj&!Koe&!`oK#,eoj1#6jo`'!PpA#'joj'#'-%-'#-#Ka,$AnnkA$6A,!61%6B6&!,G,%'7,#'",
"7,%,7,$8#Ka6$;aFxwv&;p;tZp;$UpA&!;pK&$FpU$;p`%Po`%`oP&1oj&'joA&!Aoj'!Poj1%6pF!)%)xxvut$;p;t'eoj1#'`p6%FpA&$6p`'#1joe'%1p",
"ZBZp1!1CKpC,!;a'&!'a;#P1&!6Kxxw&;p;t!1pe'#,joj,#6pUE6#,joj,#'ep1&!;jrjA#KsrAxxwut#;p;t#ApZ$6p`'!;sqZ#`p6$ZpA&$'FQF,$6ON1",
"xxwut#'-'t$KaA$AaF!1OMA!AaF$AaKxxwvt&",
0 };
/* LG_FONT2X_END */
#endif

typedef struct { float x, y, w, h; } lg__rect;

typedef struct {
    float x0, x1;                   /* left and right edge of the contents */
    float cy;                       /* top of the next line */
    float line_x, line_y, line_h;   /* right end, top and height of the line being filled */
    float indent, max_x, max_y;
    int same_line, has_item;
} lg__layout;

typedef struct lg__box {            /* a window or a panel */
    unsigned int id;
    lg__rect rect, inner;
    float scroll, content_w, content_h, pad;
    int z, flags, last_frame, is_window, is_popup, has_bar;
    int placed;                     /* it has been given its first position */
    int back;                       /* stays behind floating windows: 1 fills the screen, 2 docked or the main menu bar */
    int dock, order, hidden;        /* the place it is docked in (number + 1), its turn among the tabs, behind another tab */
    int bar;                        /* the frame in which it last asked for a menu bar */
    float fw, fh, tab_w, bar_y, bar_h;      /* its size while floating, the width of its tab, where its menu bar is */
    lg__layout lay;
    struct lg__box *parent;
} lg__box;

/* The screen is cut into places for docked windows: a tree, where a place is either split
   into two (side by side, or one above the other) or holds windows as tabs. One place is
   the hole: the middle that is left to the program. */
enum { LG__D_FREE, LG__D_HOLE, LG__D_LEAF, LG__D_COLS, LG__D_ROWS };
typedef struct {
    int kind, parent, a, b, live, centre;   /* a, b: the halves of a split. live: something in it is shown. centre: a leaf that was the hole */
    float share;                    /* how much of a split goes to a */
    unsigned int tab, want;         /* the window in front, and the one that was asked to come to the front */
    lg__rect rect;
    lg__rect fr;                    /* a group that floats (a tree of its own: parent -1, not place 0): where it is */
    int z;                          /* ...and how far up, counted like the windows' z */
} lg__dock;

typedef struct { char *text; int caret, sel; } lg__snap;        /* a text as it was, for undo */

typedef struct {                    /* what a popup interrupts */
    lg__box *box, *win;
    int layer, nids, nclips, item_hovered, item_pressed, item_key;
    unsigned int item_id;
    lg__rect item_rect;
} lg__saved;

typedef struct {
    int on, cols, col, row, flags, bg;
    float x0, x1, indent, top, row_y, bottom;
    float colx[LOGUIA_MAX_COLUMNS + 1];
    unsigned int id;
} lg__table;

typedef struct {
    lg_style style;
    lg_font font;
    unsigned char *own_pixels, *own_pixels2;
    float pixel_scale;
    float w, h, dt;
    double time;
    int frame;

    float mx, my, pmx, pmy, wheel;
    float grab_x, grab_y;       /* where the held thing was grabbed, so a drag never drifts from the mouse */
    float press_x, press_y;     /* where the mouse was when the button went down */
    int mdown[3], mpressed[3], mreleased[3];
    unsigned char key_down[LG_KEY_COUNT], key_pressed[LG_KEY_COUNT];
    unsigned char key_ctrl[LG_KEY_COUNT], key_shift[LG_KEY_COUNT];     /* Ctrl / Shift was held at the moment the key went down */
    unsigned int chars[32];
    int nchars;
    const char *(*clip_get)(void *);
    void (*clip_set)(void *, const char *);
    void *clip_user;

    unsigned int ids[LOGUIA_ID_STACK];
    int nids;
    unsigned int active, focus;
    int focus_keep, focus_seen;
    unsigned int item_id;
    lg__rect item_rect;
    int item_hovered, item_pressed, item_key;

    /* keyboard navigation: nav_id is the marked widget, nav_req asks for the next (+1) or previous (-1) one */
    unsigned int nav_id, nav_first, nav_last, nav_arrived;
    int nav_off, nav_show, nav_req, nav_take, nav_wrap, nav_scroll, nav_auto;

    unsigned int popups[LOGUIA_MAX_POPUPS], popup_nav[LOGUIA_MAX_POPUPS], popup_closed;
    float popup_x[LOGUIA_MAX_POPUPS], popup_y[LOGUIA_MAX_POPUPS], popup_min_w[LOGUIA_MAX_POPUPS];
    int popup_seen[LOGUIA_MAX_POPUPS], npopups, pdepth;
    lg__saved saved[LOGUIA_MAX_POPUPS];
    lg__table table;
    unsigned int click_id;
    double click_time;

    lg__box boxes[LOGUIA_MAX_BOXES];
    lg__box *box, *win, *hover_win, *scroll_box;
    int top_z, layer;
    float next_w;

    lg__rect clips[LOGUIA_CLIP_STACK];
    int nclips;

    lg_vertex *v;       int nv, capv;
    unsigned int *idx;  int ni, capi;
    lg_cmd *cmds;       int *cmd_layer; int ncmd, capcmd;
    lg_draw_list list;

    unsigned int st_key[LOGUIA_STATE_SLOTS];
    int st_val[LOGUIA_STATE_SLOTS];

    int caret, sel, edit_multi;     /* the text field being edited */
    float edit_scroll;
    lg__snap undo[LOGUIA_UNDO_LEVELS], redo[LOGUIA_UNDO_LEVELS];
    int nundo, nredo, undo_kind;
    unsigned int undo_id;
    double undo_time;

    char compose[256];              /* what an input method is composing, and where its caret is */
    int compose_caret, ime_frame;
    lg__rect ime;                   /* the caret of the field being typed into */

    unsigned int num_id, num_wait;  /* the slider whose number is being typed; the one whose number was pressed */
    int num_frame;
    char num[32];

    lg__dock docks[LOGUIA_MAX_DOCKS];
    int dock_order;
    unsigned int drop_id;           /* a window the mouse docked (drop_side 1..5, into drop_node) or pulled out (-1) */
    unsigned int drop_other;        /* drop_node -1: it was dropped onto this floating window */
    int drop_node, drop_side;

    unsigned int menu_pid;          /* the open menu of a menu bar */
    int in_bar, main_bar, bar_frame;
    float bar_h;                    /* the height of the main menu bar */
    lg__layout bar_lay;
} lg__ctx;

static lg__ctx lg__c;

/* ---------------------------------------------------------------- small helpers */

static float lg__min(float a, float b) { return a < b ? a : b; }
static float lg__max(float a, float b) { return a > b ? a : b; }
static float lg__clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float lg__floor(float v) { int i = (int)v; return (float)(v < (float)i ? i - 1 : i); }
static float lg__sqrt(float v) {
    float r = v > 1 ? v : 1;
    int i;
    if (v <= 0) return 0;
    for (i = 0; i < 24; i++) r = 0.5f * (r + v / r);
    return r;
}
static lg__rect lg__r(float x, float y, float w, float h) { lg__rect r; r.x = x; r.y = y; r.w = w; r.h = h; return r; }
static int lg__in(lg__rect r, float x, float y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }
static unsigned int lg__col(int which) { return lg__c.style.colors[which]; }

static unsigned int lg__hash(const void *data, int len, unsigned int seed) {
    const unsigned char *p = (const unsigned char *)data;
    unsigned int h = seed;
    int i;
    for (i = 0; i < len; i++) h = (h ^ p[i]) * 16777619u;
    return h ? h : 1;
}
static unsigned int lg__seed(void) { return lg__c.nids ? lg__c.ids[lg__c.nids - 1] : 2166136261u; }
static unsigned int lg__id(const char *label) { return lg__hash(label, (int)strlen(label), lg__seed()); }
static void lg__push(unsigned int id) {
    LOGUIA_ASSERT(lg__c.nids < LOGUIA_ID_STACK);
    lg__c.ids[lg__c.nids++] = id;
}
/* where the shown part of a label stops ("Name##hidden") */
static const char *lg__label_end(const char *s) {
    const char *e = strstr(s, "##");
    return e ? e : s + strlen(s);
}

/* remembered yes/no values (open tree nodes), found by id */
static int *lg__state(unsigned int id, int initial) {
    unsigned int i = id & (LOGUIA_STATE_SLOTS - 1), n;
    for (n = 0; n < LOGUIA_STATE_SLOTS; n++, i = (i + 1) & (LOGUIA_STATE_SLOTS - 1)) {
        if (lg__c.st_key[i] == id) return &lg__c.st_val[i];
        if (lg__c.st_key[i] == 0) { lg__c.st_key[i] = id; lg__c.st_val[i] = initial; return &lg__c.st_val[i]; }
    }
    memset(lg__c.st_key, 0, sizeof lg__c.st_key);       /* full: forget everything and start again */
    return lg__state(id, initial);
}

/* ---------------------------------------------------------------- style */

static void lg__metrics(void) {
    lg_style *s = &lg__c.style;
    s->padding_x = 8; s->padding_y = 4; s->spacing_x = 8; s->spacing_y = 6;
    s->window_padding = 10; s->indent = 18; s->scrollbar_size = 10; s->border = 1; s->rounding = 4;
    if (s->font_scale <= 0) s->font_scale = 1;
}
LGDEF void lg_style_dark(void) {
    unsigned int *c = lg__c.style.colors;
    lg__metrics();
    c[LG_COL_TEXT] = 0xE6E9EFFF;          c[LG_COL_TEXT_DIM] = 0x8B93A5FF;
    c[LG_COL_WINDOW] = 0x1B1E26FF;        c[LG_COL_TITLE] = 0x252A35FF;
    c[LG_COL_TITLE_ACTIVE] = 0x2F3A52FF;  c[LG_COL_BORDER] = 0x3A4152FF;
    c[LG_COL_PANEL] = 0x15171DFF;         c[LG_COL_BUTTON] = 0x34405AFF;
    c[LG_COL_BUTTON_HOVER] = 0x43547AFF;  c[LG_COL_BUTTON_ACTIVE] = 0x5068A0FF;
    c[LG_COL_FRAME] = 0x11131AFF;         c[LG_COL_FRAME_HOVER] = 0x1C202CFF;
    c[LG_COL_ACCENT] = 0x5B9DFFFF;        c[LG_COL_SELECTED] = 0x2F4A7AFF;
    c[LG_COL_HOVER] = 0xFFFFFF14;         c[LG_COL_SEPARATOR] = 0x3A4152FF;
    c[LG_COL_SCROLLBAR] = 0x00000040;     c[LG_COL_SCROLL_THUMB] = 0x4A5266FF;
    c[LG_COL_SCROLL_THUMB_HOVER] = 0x6A7590FF; c[LG_COL_TEXT_SELECT] = 0x5B9DFF66;
}
LGDEF void lg_style_light(void) {
    unsigned int *c = lg__c.style.colors;
    lg__metrics();
    c[LG_COL_TEXT] = 0x1F2430FF;          c[LG_COL_TEXT_DIM] = 0x6B7385FF;
    c[LG_COL_WINDOW] = 0xF4F5F8FF;        c[LG_COL_TITLE] = 0xDDE1EAFF;
    c[LG_COL_TITLE_ACTIVE] = 0xC3D4F5FF;  c[LG_COL_BORDER] = 0xB4BBCBFF;
    c[LG_COL_PANEL] = 0xFFFFFFFF;         c[LG_COL_BUTTON] = 0xD5DCEBFF;
    c[LG_COL_BUTTON_HOVER] = 0xC0CCE6FF;  c[LG_COL_BUTTON_ACTIVE] = 0xA5B8E0FF;
    c[LG_COL_FRAME] = 0xFFFFFFFF;         c[LG_COL_FRAME_HOVER] = 0xEDF1F9FF;
    c[LG_COL_ACCENT] = 0x2F6FE0FF;        c[LG_COL_SELECTED] = 0xBFD4FAFF;
    c[LG_COL_HOVER] = 0x00000012;         c[LG_COL_SEPARATOR] = 0xB4BBCBFF;
    c[LG_COL_SCROLLBAR] = 0x00000018;     c[LG_COL_SCROLL_THUMB] = 0xA9B1C4FF;
    c[LG_COL_SCROLL_THUMB_HOVER] = 0x8792ABFF; c[LG_COL_TEXT_SELECT] = 0x2F6FE055;
}
LGDEF lg_style *lg_get_style(void) { return &lg__c.style; }
LGDEF void lg_set_color(int which, unsigned int rgba) {
    if (which >= 0 && which < LG_COL_COUNT) lg__c.style.colors[which] = rgba;
}
LGDEF unsigned int lg_rgba(int r, int g, int b, int a) {
    return ((unsigned int)(r & 255) << 24) | ((unsigned int)(g & 255) << 16) | ((unsigned int)(b & 255) << 8) | (unsigned int)(a & 255);
}

/* ---------------------------------------------------------------- font */

#ifndef LOGUIA_NO_DEFAULT_FONT
static unsigned char *lg__unpack_font(const char *const *rle, int total) {
    /* printable ASCII without  "  \  ?   (the same list is in tools/bake_font.c) */
    static const char alphabet[] =
        "!#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz";
    static const int zero_run[] = { 10, 20, 40, 80, 160 };
    unsigned char code[128];
    int p = 0, i;
    const char *s;
    unsigned char *px = (unsigned char *)LOGUIA_MALLOC((size_t)total);
    memset(px, 0, (size_t)total);
    memset(code, 0, sizeof code);
    for (i = 0; alphabet[i]; i++) code[(int)alphabet[i]] = (unsigned char)i;
    for (i = 0; rle[i]; i++)
        for (s = rle[i]; *s; s++) {
            int k = code[*s & 127];
            if (k >= 80) p += zero_run[k - 80];
            else {
                int run = k % 5 + 1;
                if (p + run <= total) memset(px + p, (k / 5) * 17, (size_t)run);
                p += run;
            }
        }
    return px;
}
/* the built-in font: the normal one, or the double-size one for HiDPI screens */
static void lg__default_font(int big) {
    lg__ctx *c = &lg__c;
    c->font.source = 0;
#ifdef LG__FONT2_GLYPHS
    if (big) {
        if (!c->own_pixels2) c->own_pixels2 = lg__unpack_font(lg__font2_rle, LG__ATLAS2_W * LG__ATLAS2_H);
        c->font.pixels = c->own_pixels2;
        c->font.width = LG__ATLAS2_W; c->font.height = LG__ATLAS2_H;
        c->font.glyphs = lg__glyphs2; c->font.glyph_count = LG__FONT2_GLYPHS;
        c->font.line_height = LG__FONT2_LINE; c->font.scale = 2;
        return;
    }
#endif
    (void)big;
    if (!c->own_pixels) c->own_pixels = lg__unpack_font(lg__font_rle, LG__ATLAS_W * LG__ATLAS_H);
    c->font.pixels = c->own_pixels;
    c->font.width = LG__ATLAS_W; c->font.height = LG__ATLAS_H;
    c->font.glyphs = lg__glyphs; c->font.glyph_count = LG__FONT_GLYPHS;
    c->font.line_height = LG__FONT_LINE; c->font.scale = 1;
}
#endif

/* ---------------------------------------------------------------- fonts from TrueType and OpenType files */

/* a piece of the file, and a place in it; reading past its end gives zeros */
typedef struct { const unsigned char *p; int n, at; } lg__rd;

static lg__rd lg__rd_make(const unsigned char *p, int n) { lg__rd r; r.p = p; r.n = n < 0 ? 0 : n; r.at = 0; return r; }
static int lg__rd8(lg__rd *r) { int v = r->at >= 0 && r->at < r->n ? r->p[r->at] : 0; r->at++; return v; }
static unsigned int lg__rdn(lg__rd *r, int bytes) { unsigned int v = 0; while (bytes-- > 0) v = v << 8 | (unsigned int)lg__rd8(r); return v; }
static lg__rd lg__rd_part(const lg__rd *r, int at, int n) {
    if (at < 0 || n < 0 || at > r->n || n > r->n - at) return lg__rd_make(r->p, 0);
    return lg__rd_make(r->p + at, n);
}

typedef struct lg__tt {
    lg_font font;                   /* what lg_set_font copies */
    const unsigned char *data;
    int size;
    void *own_data;                 /* the file, when lg_font_load_file read it */
    unsigned int glyf, loca, hmtx, cmap;
    int cmap_format, loca_long, glyphs_total, hmetrics, units, ascent, descent, gap;
    int cff;                        /* 0: TrueType outlines, 1: CFF, 2: CFF2 */
    lg__rd cfftab, gsubrs, charstrings, subrs, fdarray, fdselect, vstore;
    float pixels, scale, k, top;    /* the size; the screen scale it is drawn for; font units to picture pixels; the baseline */
    struct lg__tt *next;            /* where missing characters come from */
    lg_glyph *glyphs;
    int count, cap, pen_x, pen_y, row_h, full;
    unsigned char *pixels_own;
    float *acc;                     /* the rasteriser's work area */
    int acc_cap;
} lg__tt;

static unsigned int lg__t16(const lg__tt *f, unsigned int at) { return at + 2 <= (unsigned int)f->size ? (unsigned int)(f->data[at] << 8 | f->data[at + 1]) : 0; }
static unsigned int lg__t32(const lg__tt *f, unsigned int at) { return at + 4 <= (unsigned int)f->size ? lg__t16(f, at) << 16 | lg__t16(f, at + 2) : 0; }
static int lg__ts16(const lg__tt *f, unsigned int at) { return (int)(short)lg__t16(f, at); }
static unsigned int lg__tt_table(const lg__tt *f, unsigned int font, const char *tag, unsigned int *len) {
    unsigned int count = lg__t16(f, font + 4), i;
    for (i = 0; i < count; i++) {
        unsigned int rec = font + 12 + 16 * i, at, n;
        if (rec + 16 > (unsigned int)f->size) break;
        if (memcmp(f->data + rec, tag, 4) != 0) continue;
        at = lg__t32(f, rec + 8); n = lg__t32(f, rec + 12);
        if (at > (unsigned int)f->size || n > (unsigned int)f->size - at) return 0;
        if (len) *len = n;
        return at;
    }
    return 0;
}

/* which of the font's glyphs draws the character cp; 0 for none */
static int lg__tt_gid(const lg__tt *f, unsigned int cp) {
    unsigned int t = f->cmap, i;
    if (f->cmap_format == 12) {
        unsigned int lo = 0, hi = lg__t32(f, t + 12);
        while (lo < hi) {
            unsigned int mid = (lo + hi) / 2, g = t + 16 + 12 * mid;
            if (cp < lg__t32(f, g)) hi = mid;
            else if (cp > lg__t32(f, g + 4)) lo = mid + 1;
            else return (int)(lg__t32(f, g + 8) + cp - lg__t32(f, g));
        }
        return 0;
    }
    if (f->cmap_format == 4 && cp <= 0xFFFF) {
        unsigned int segs = lg__t16(f, t + 6) / 2, ends = t + 14, starts = ends + 2 * segs + 2, deltas = starts + 2 * segs, ranges = deltas + 2 * segs;
        for (i = 0; i < segs; i++) {
            unsigned int start, delta, ro, g;
            if (cp > lg__t16(f, ends + 2 * i)) continue;
            start = lg__t16(f, starts + 2 * i);
            if (cp < start) return 0;
            delta = lg__t16(f, deltas + 2 * i); ro = lg__t16(f, ranges + 2 * i);
            if (!ro) return (int)((cp + delta) & 0xFFFF);
            g = lg__t16(f, ranges + 2 * i + ro + 2 * (cp - start));
            return g ? (int)((g + delta) & 0xFFFF) : 0;
        }
    }
    return 0;
}

/* An outline becomes straight edges in picture pixels (y going down); curves are cut into short pieces. */
typedef struct { float *seg; int n, cap, open; float k, cx, cy, sx, sy; } lg__path;

static void lg__path_seg(lg__path *p, float x0, float y0, float x1, float y1) {
    if (p->n == p->cap) {
        p->cap = p->cap * 2 + 64;
        p->seg = (float *)LOGUIA_REALLOC(p->seg, (size_t)p->cap * 4 * sizeof(float));
    }
    p->seg[4 * p->n] = x0; p->seg[4 * p->n + 1] = y0; p->seg[4 * p->n + 2] = x1; p->seg[4 * p->n + 3] = y1;
    p->n++;
}
static void lg__path_close(lg__path *p) {
    if (p->open && (p->cx != p->sx || p->cy != p->sy)) lg__path_seg(p, p->cx, p->cy, p->sx, p->sy);
    p->open = 0;
}
/* these take font units */
static void lg__path_move(lg__path *p, float x, float y) { lg__path_close(p); p->cx = p->sx = x * p->k; p->cy = p->sy = -y * p->k; p->open = 1; }
static void lg__path_line(lg__path *p, float x, float y) {
    float X = x * p->k, Y = -y * p->k;
    lg__path_seg(p, p->cx, p->cy, X, Y);
    p->cx = X; p->cy = Y;
}
static int lg__path_steps(float d) { int n = (int)lg__sqrt(d * 2) + 1; return n < 1 ? 1 : n > 24 ? 24 : n; }
static float lg__absf(float v) { return v < 0 ? -v : v; }
static void lg__path_quad(lg__path *p, float qx, float qy, float x, float y) {
    float x0 = p->cx, y0 = p->cy, x1 = qx * p->k, y1 = -qy * p->k, x2 = x * p->k, y2 = -y * p->k;
    int n = lg__path_steps(lg__absf(x1 - x0) + lg__absf(y1 - y0) + lg__absf(x2 - x1) + lg__absf(y2 - y1)), i;
    for (i = 1; i <= n; i++) {
        float t = (float)i / (float)n, u = 1 - t;
        float X = u * u * x0 + 2 * u * t * x1 + t * t * x2, Y = u * u * y0 + 2 * u * t * y1 + t * t * y2;
        lg__path_seg(p, p->cx, p->cy, X, Y);
        p->cx = X; p->cy = Y;
    }
}
static void lg__path_cubic(lg__path *p, float ax, float ay, float bx, float by, float x, float y) {
    float x0 = p->cx, y0 = p->cy, x1 = ax * p->k, y1 = -ay * p->k, x2 = bx * p->k, y2 = -by * p->k, x3 = x * p->k, y3 = -y * p->k;
    int n = lg__path_steps(lg__absf(x1 - x0) + lg__absf(y1 - y0) + lg__absf(x2 - x1) + lg__absf(y2 - y1) + lg__absf(x3 - x2) + lg__absf(y3 - y2)), i;
    for (i = 1; i <= n; i++) {
        float t = (float)i / (float)n, u = 1 - t;
        float X = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
        float Y = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
        lg__path_seg(p, p->cx, p->cy, X, Y);
        p->cx = X; p->cy = Y;
    }
}

/* --- TrueType outlines (the glyf table): straight lines and quadratic curves */
static void lg__glyf_outline(const lg__tt *f, int gid, lg__path *p, const float *m, int depth) {
    unsigned int a, b, g;
    int contours;
    if (gid < 0 || gid >= f->glyphs_total || depth > 8) return;
    if (f->loca_long) { a = lg__t32(f, f->loca + 4 * (unsigned int)gid); b = lg__t32(f, f->loca + 4 * (unsigned int)gid + 4); }
    else { a = 2 * lg__t16(f, f->loca + 2 * (unsigned int)gid); b = 2 * lg__t16(f, f->loca + 2 * (unsigned int)gid + 2); }
    if (a >= b) return;                 /* an empty glyph: a space */
    g = f->glyf + a;
    contours = lg__ts16(f, g);
    if (contours >= 0) {
        unsigned int ends = g + 10, at;
        int points = contours ? (int)lg__t16(f, ends + 2 * (unsigned int)(contours - 1)) + 1 : 0, i, c, s0 = 0;
        unsigned char *fl;
        float *xy;
        int v = 0;
        if (points <= 0 || points > 0xFFFF) return;
        fl = (unsigned char *)LOGUIA_MALLOC((size_t)points);
        xy = (float *)LOGUIA_MALLOC((size_t)points * 2 * sizeof(float));
        at = ends + 2 * (unsigned int)contours;
        at += 2 + lg__t16(f, at);       /* past the instructions */
        for (i = 0; i < points;) {      /* flags, some of them repeated */
            int flag = at < (unsigned int)f->size ? f->data[at++] : 0, rep = 0;
            if (flag & 8) rep = at < (unsigned int)f->size ? f->data[at++] : 0;
            for (; rep >= 0 && i < points; rep--) fl[i++] = (unsigned char)flag;
        }
        for (i = 0; i < points; i++) {  /* x, then y: each a change from the point before */
            int flag = fl[i];
            if (flag & 2) { int d = at < (unsigned int)f->size ? f->data[at] : 0; at++; v += flag & 16 ? d : -d; }
            else if (!(flag & 16)) { v += lg__ts16(f, at); at += 2; }
            xy[2 * i] = (float)v;
        }
        for (i = 0, v = 0; i < points; i++) {
            int flag = fl[i];
            if (flag & 4) { int d = at < (unsigned int)f->size ? f->data[at] : 0; at++; v += flag & 32 ? d : -d; }
            else if (!(flag & 32)) { v += lg__ts16(f, at); at += 2; }
            xy[2 * i + 1] = (float)v;
        }
        for (i = 0; i < points; i++) {  /* through the transform of a composite glyph */
            float x = xy[2 * i], y = xy[2 * i + 1];
            xy[2 * i] = m[0] * x + m[2] * y + m[4];
            xy[2 * i + 1] = m[1] * x + m[3] * y + m[5];
        }
        for (c = 0; c < contours; c++) {
            int e = (int)lg__t16(f, ends + 2 * (unsigned int)c), n = e - s0 + 1, first = -1, j, pend = 0;
            float px = 0, py = 0, sx, sy;
            if (n <= 0 || e >= points) break;
            for (j = 0; j < n; j++) if (fl[s0 + j] & 1) { first = j; break; }
            /* start on a point that is on the curve; when there is none, half-way between the last and the first */
            if (first >= 0) { sx = xy[2 * (s0 + first)]; sy = xy[2 * (s0 + first) + 1]; }
            else { sx = (xy[2 * s0] + xy[2 * e]) / 2; sy = (xy[2 * s0 + 1] + xy[2 * e + 1]) / 2; }
            lg__path_move(p, sx, sy);
            for (j = first >= 0 ? 1 : 0; j <= (first >= 0 ? n : n - 1); j++) {
                int i2 = s0 + (first >= 0 ? (first + j) % n : j);
                float x = xy[2 * i2], y = xy[2 * i2 + 1];
                if (fl[i2] & 1) {
                    if (pend) lg__path_quad(p, px, py, x, y); else lg__path_line(p, x, y);
                    pend = 0;
                } else {
                    if (pend) lg__path_quad(p, px, py, (px + x) / 2, (py + y) / 2);     /* two off points: one on point is between them */
                    px = x; py = y; pend = 1;
                }
            }
            if (pend) lg__path_quad(p, px, py, sx, sy);
            lg__path_close(p);
            s0 = e + 1;
        }
        LOGUIA_FREE(fl); LOGUIA_FREE(xy);
    } else {                            /* made of other glyphs, each moved, maybe turned or scaled */
        unsigned int at = g + 10, flags;
        do {
            float t[4] = { 1, 0, 0, 1 }, dx, dy, mm[6];
            int child;
            flags = lg__t16(f, at); child = (int)lg__t16(f, at + 2); at += 4;
            if (flags & 1) { dx = (float)lg__ts16(f, at); dy = (float)lg__ts16(f, at + 2); at += 4; }
            else { dx = (float)(signed char)(at < (unsigned int)f->size ? f->data[at] : 0); dy = (float)(signed char)(at + 1 < (unsigned int)f->size ? f->data[at + 1] : 0); at += 2; }
            if (!(flags & 2)) dx = dy = 0;      /* matched points: rare, left where they are */
            if (flags & 8) { t[0] = t[3] = (float)lg__ts16(f, at) / 16384; at += 2; }
            else if (flags & 0x40) { t[0] = (float)lg__ts16(f, at) / 16384; t[3] = (float)lg__ts16(f, at + 2) / 16384; at += 4; }
            else if (flags & 0x80) { t[0] = (float)lg__ts16(f, at) / 16384; t[1] = (float)lg__ts16(f, at + 2) / 16384; t[2] = (float)lg__ts16(f, at + 4) / 16384; t[3] = (float)lg__ts16(f, at + 6) / 16384; at += 8; }
            mm[0] = m[0] * t[0] + m[2] * t[1]; mm[1] = m[1] * t[0] + m[3] * t[1];
            mm[2] = m[0] * t[2] + m[2] * t[3]; mm[3] = m[1] * t[2] + m[3] * t[3];
            mm[4] = m[0] * dx + m[2] * dy + m[4]; mm[5] = m[1] * dx + m[3] * dy + m[5];
            lg__glyf_outline(f, child, p, mm, depth + 1);
        } while ((flags & 0x20) && at < (unsigned int)f->size);
    }
}

/* --- CFF and CFF2 outlines (OpenType .otf): little programs that draw lines and cubic curves */
static lg__rd lg__cff_index(lg__rd *r, int ver) {      /* an INDEX at r's place; r moves past it */
    int start = r->at, count = (int)lg__rdn(r, ver == 2 ? 4 : 2), size, last;
    if (count <= 0) return lg__rd_part(r, start, r->at - start);
    size = lg__rd8(r);
    if (size < 1 || size > 4) { r->at = r->n; return lg__rd_make(r->p, 0); }
    r->at += size * count;
    last = (int)lg__rdn(r, size);
    r->at += last - 1;
    return lg__rd_part(r, start, r->at - start);
}
static int lg__cff_count(lg__rd idx, int ver) { idx.at = 0; return (int)lg__rdn(&idx, ver == 2 ? 4 : 2); }
static lg__rd lg__cff_get(lg__rd idx, int ver, int i) {
    int cs = ver == 2 ? 4 : 2, count, size, a, b;
    idx.at = 0;
    count = (int)lg__rdn(&idx, cs); size = lg__rd8(&idx);
    if (i < 0 || i >= count || size < 1 || size > 4) return lg__rd_make(idx.p, 0);
    idx.at = cs + 1 + i * size;
    a = (int)lg__rdn(&idx, size); b = (int)lg__rdn(&idx, size);
    return lg__rd_part(&idx, cs + (count + 1) * size + a, b - a);
}
static lg__rd lg__cff_index_at(lg__rd table, int at, int ver) {
    lg__rd r = table;
    if (at <= 0 || at >= table.n) return lg__rd_make(table.p, 0);
    r.at = at;
    return lg__cff_index(&r, ver);
}
/* the numbers in front of the key in a DICT; returns how many there were */
static int lg__dict(lg__rd d, int key, int max, int *out) {
    d.at = 0;
    while (d.at < d.n) {
        int n = 0, op;
        while (d.at < d.n && d.p[d.at] >= 28 && d.p[d.at] != 31 && d.p[d.at] != 255) {
            int b0 = lg__rd8(&d), v = 0;
            if (b0 == 30) {             /* a real number: skipped, Loguia needs none */
                while (d.at < d.n) { int b = lg__rd8(&d); if ((b & 15) == 15 || (b >> 4) == 15) break; }
            } else if (b0 == 28) v = (int)(short)lg__rdn(&d, 2);
            else if (b0 == 29) v = (int)lg__rdn(&d, 4);
            else if (b0 < 247) v = b0 - 139;
            else if (b0 < 251) v = (b0 - 247) * 256 + lg__rd8(&d) + 108;
            else v = -(b0 - 251) * 256 - lg__rd8(&d) - 108;
            if (n < max) out[n] = v;
            n++;
        }
        if (d.at >= d.n) break;
        op = lg__rd8(&d);
        if (op == 12) op = 0x100 | lg__rd8(&d);
        if (op == key) return n;
    }
    return 0;
}
static int lg__cff_bias(int count) { return count < 1240 ? 107 : count < 33900 ? 1131 : 32768; }
/* how many numbers each blended value carries in a CFF2 font, for the variation data number vs */
static int lg__cff_regions(const lg__tt *f, int vs) {
    lg__rd v = f->vstore;
    int count, at;
    v.at = 2 + 6;                       /* the length, the format and the region list come first */
    count = (int)lg__rdn(&v, 2);
    if (vs < 0 || vs >= count) return 0;
    v.at = 2 + 8 + 4 * vs;
    at = (int)lg__rdn(&v, 4);
    v.at = 2 + at + 4;
    return (int)lg__rdn(&v, 2);
}
/* a CID-keyed or CFF2 font keeps its subroutines per group of glyphs: the group of glyph gid */
static int lg__cff_fd(const lg__tt *f, int gid) {
    lg__rd r = f->fdselect;
    int format, n, i;
    if (!r.n) return 0;
    format = lg__rd8(&r);
    if (format == 0) { r.at = 1 + gid; return lg__rd8(&r); }
    if (format == 3 || format == 4) {
        int w = format == 3 ? 2 : 4, first, fd, next;
        n = (int)lg__rdn(&r, w);
        first = (int)lg__rdn(&r, w);
        for (i = 0; i < n; i++) {
            fd = (int)lg__rdn(&r, format == 3 ? 1 : 2);
            next = (int)lg__rdn(&r, w);
            if (gid >= first && gid < next) return fd;
            first = next;
        }
    }
    return 0;
}
static void lg__cff_curve(lg__path *p, float *x, float *y, float d1, float d2, float d3, float d4, float d5, float d6) {
    float ax = *x + d1, ay = *y + d2, bx = ax + d3, by = ay + d4;
    *x = bx + d5; *y = by + d6;
    lg__path_cubic(p, ax, ay, bx, by, *x, *y);
}
static void lg__cff_line(lg__path *p, float *x, float *y, float dx, float dy) { *x += dx; *y += dy; lg__path_line(p, *x, *y); }

static int lg__cff_outline(const lg__tt *f, int gid, lg__path *p) {
    lg__rd b = lg__cff_get(f->charstrings, f->cff, gid), subrs = f->subrs, stack[10];
    float s[513], x = 0, y = 0;
    int sp = 0, depth = 0, header = 1, maskbits = 0, vs = 0, i, clear;
    if (f->fdarray.n) {                 /* the subroutines of this glyph's group */
        lg__rd fd = lg__cff_get(f->fdarray, f->cff, lg__cff_fd(f, gid));
        int pv[2] = { 0, 0 }, so = 0, vv = 0;
        if (lg__dict(fd, 18, 2, pv) >= 2) {
            lg__rd priv = lg__rd_part(&f->cfftab, pv[1], pv[0]);
            if (lg__dict(priv, 19, 1, &so) >= 1) subrs = lg__cff_index_at(f->cfftab, pv[1] + so, f->cff);
            else subrs = lg__rd_make(priv.p, 0);
            if (lg__dict(priv, 22, 1, &vv) >= 1) vs = vv;
        }
    }
    if (!b.n) return 0;
    for (;;) {
        int b0;
        if (b.at >= b.n) {              /* the end of a subroutine (CFF2 has no return) or of the glyph */
            if (depth > 0) { b = stack[--depth]; continue; }
            break;
        }
        b0 = lg__rd8(&b);
        i = 0; clear = 1;
        switch (b0) {
        case 19: case 20:               /* hintmask, cntrmask: hints, skipped */
            if (header) maskbits += sp / 2;
            header = 0;
            b.at += (maskbits + 7) / 8;
            break;
        case 1: case 3: case 18: case 23: maskbits += sp / 2; break;       /* stems: only counted */
        case 21: if (sp < 2) return 0; header = 0; lg__path_close(p); x += s[sp - 2]; y += s[sp - 1]; lg__path_move(p, x, y); break;
        case 4: if (sp < 1) return 0; header = 0; lg__path_close(p); y += s[sp - 1]; lg__path_move(p, x, y); break;
        case 22: if (sp < 1) return 0; header = 0; lg__path_close(p); x += s[sp - 1]; lg__path_move(p, x, y); break;
        case 5: for (; i + 1 < sp; i += 2) lg__cff_line(p, &x, &y, s[i], s[i + 1]); break;
        case 6: case 7: {               /* hlineto, vlineto: the lines take turns */
            int h = b0 == 6;
            for (; i < sp; i++, h = !h) lg__cff_line(p, &x, &y, h ? s[i] : 0, h ? 0 : s[i]);
            break;
        }
        case 30: case 31: {             /* vhcurveto, hvcurveto */
            int h = b0 == 31;
            for (; i + 3 < sp; i += 4, h = !h) {
                float last = sp - i == 5 ? s[i + 4] : 0;
                if (h) lg__cff_curve(p, &x, &y, s[i], 0, s[i + 1], s[i + 2], last, s[i + 3]);
                else lg__cff_curve(p, &x, &y, 0, s[i], s[i + 1], s[i + 2], s[i + 3], last);
            }
            break;
        }
        case 8: for (; i + 5 < sp; i += 6) lg__cff_curve(p, &x, &y, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]); break;
        case 24:                        /* rcurveline */
            for (; i + 5 < sp - 2; i += 6) lg__cff_curve(p, &x, &y, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            if (i + 1 < sp) lg__cff_line(p, &x, &y, s[i], s[i + 1]);
            break;
        case 25:                        /* rlinecurve */
            for (; i + 1 < sp - 6; i += 2) lg__cff_line(p, &x, &y, s[i], s[i + 1]);
            if (i + 5 < sp) lg__cff_curve(p, &x, &y, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;
        case 26: case 27: {             /* vvcurveto, hhcurveto */
            float first = 0;
            if (sp & 1) first = s[i++];
            for (; i + 3 < sp; i += 4, first = 0) {
                if (b0 == 27) lg__cff_curve(p, &x, &y, s[i], first, s[i + 1], s[i + 2], s[i + 3], 0);
                else lg__cff_curve(p, &x, &y, first, s[i], s[i + 1], s[i + 2], 0, s[i + 3]);
            }
            break;
        }
        case 10: case 29: {             /* callsubr, callgsubr */
            lg__rd list = b0 == 10 ? subrs : f->gsubrs;
            int v;
            if (sp < 1 || depth >= 10) return 0;
            v = (int)s[--sp] + lg__cff_bias(lg__cff_count(list, f->cff));
            stack[depth++] = b;
            b = lg__cff_get(list, f->cff, v);
            if (!b.n) return 0;
            clear = 0;
            break;
        }
        case 11: if (depth <= 0) return 0; b = stack[--depth]; clear = 0; break;    /* return */
        case 14: lg__path_close(p); return 1;                                       /* endchar */
        case 15: if (sp >= 1) vs = (int)s[sp - 1]; break;                          /* vsindex (CFF2) */
        case 16: {                      /* blend (CFF2): the default values stay, the changes for other weights go */
            int n, k = lg__cff_regions(f, vs), base;
            if (sp < 1) return 0;
            n = (int)s[sp - 1];
            base = sp - 1 - n * (k + 1);
            if (n < 0 || base < 0) return 0;
            sp = base + n;
            clear = 0;
            break;
        }
        case 12: {                      /* the flex curves */
            int b1 = lg__rd8(&b);
            float dx, dy, d6;
            if (b1 == 35 && sp >= 13) {
                lg__cff_curve(p, &x, &y, s[0], s[1], s[2], s[3], s[4], s[5]);
                lg__cff_curve(p, &x, &y, s[6], s[7], s[8], s[9], s[10], s[11]);
            } else if (b1 == 34 && sp >= 7) {
                lg__cff_curve(p, &x, &y, s[0], 0, s[1], s[2], s[3], 0);
                lg__cff_curve(p, &x, &y, s[4], 0, s[5], -s[2], s[6], 0);
            } else if (b1 == 36 && sp >= 9) {
                lg__cff_curve(p, &x, &y, s[0], s[1], s[2], s[3], s[4], 0);
                lg__cff_curve(p, &x, &y, s[5], 0, s[6], s[7], s[8], -(s[1] + s[3] + s[7]));
            } else if (b1 == 37 && sp >= 11) {
                dx = s[0] + s[2] + s[4] + s[6] + s[8]; dy = s[1] + s[3] + s[5] + s[7] + s[9]; d6 = s[10];
                lg__cff_curve(p, &x, &y, s[0], s[1], s[2], s[3], s[4], s[5]);
                if (lg__absf(dx) > lg__absf(dy)) lg__cff_curve(p, &x, &y, s[6], s[7], s[8], s[9], d6, -dy);
                else lg__cff_curve(p, &x, &y, s[6], s[7], s[8], s[9], -dx, d6);
            } else return 0;
            break;
        }
        default: {                      /* a number */
            float v;
            if (b0 == 28) v = (float)(short)lg__rdn(&b, 2);
            else if (b0 == 255) v = (float)(int)lg__rdn(&b, 4) / 65536.0f;
            else if (b0 >= 32 && b0 <= 246) v = (float)(b0 - 139);
            else if (b0 >= 247 && b0 <= 250) v = (float)((b0 - 247) * 256 + lg__rd8(&b) + 108);
            else if (b0 >= 251 && b0 <= 254) v = (float)(-(b0 - 251) * 256 - lg__rd8(&b) - 108);
            else return 0;
            if (sp >= 513) return 0;
            s[sp++] = v;
            clear = 0;
        }
        }
        if (clear) sp = 0;
    }
    lg__path_close(p);
    return 1;
}

/* --- the rasteriser: how much of every pixel the edges cover, added up along each row */
static void lg__raster_line(float *a, int stride, int h, float x0, float y0, float x1, float y1) {
    float dir = 1, dxdy, x, lim = (float)(stride - 2);
    int y, yend;
    if (y0 == y1) return;
    if (y0 > y1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dir = -1; }
    dxdy = (x1 - x0) / (y1 - y0);
    x = x0;
    if (y0 < 0) { x -= y0 * dxdy; y0 = 0; }
    yend = (int)-lg__floor(-y1);
    if (yend > h) yend = h;
    for (y = (int)lg__floor(y0); y < yend; y++) {
        float *row = a + y * stride;
        float dy = lg__min((float)(y + 1), y1) - lg__max((float)y, y0), xn = x + dxdy * dy, d = dy * dir;
        float xa = lg__clamp(lg__min(x, xn), 0, lim), xb = lg__clamp(lg__max(x, xn), 0, lim), xaf = lg__floor(xa), xbc = -lg__floor(-xb);
        int xai = (int)xaf, xbi = (int)xbc;
        if (xbi <= xai + 1) {
            float mid = 0.5f * (xa + xb) - xaf;
            row[xai] += d - d * mid;
            row[xai + 1] += d * mid;
        } else {
            float sl = 1 / (xb - xa), f0 = xa - xaf, a0 = 0.5f * sl * (1 - f0) * (1 - f0), f1 = xb - xbc + 1, am = 0.5f * sl * f1 * f1;
            row[xai] += d * a0;
            if (xbi == xai + 2) row[xai + 1] += d * (1 - a0 - am);
            else {
                float a1 = sl * (1.5f - f0);
                int xi;
                row[xai + 1] += d * (a1 - a0);
                for (xi = xai + 2; xi < xbi - 1; xi++) row[xi] += d * sl;
                row[xbi - 1] += d * (1 - (a1 + (float)(xbi - xai - 3) * sl) - am);
            }
            row[xbi] += d * am;
        }
        x = xn;
    }
}

static void lg__tt_sync(lg__tt *f) {
    f->font.pixels = f->pixels_own; f->font.glyphs = f->glyphs; f->font.glyph_count = f->count;
    f->font.version++;
    if (lg__c.font.source == f) lg__c.font = f->font;
}
/* starts the picture again: for another screen scale, or when it is full */
static void lg__tt_reset(lg__tt *f, float scale) {
    float lh;
    if (scale <= 0) scale = 1;
    f->scale = scale;
    f->k = f->pixels * scale / (float)f->units;
    lh = (float)(f->ascent - f->descent + f->gap) * f->k;
    f->font.line_height = lg__floor(lh + 0.5f);
    f->top = lg__floor((float)f->ascent * f->k + (float)f->gap * f->k / 2 + 0.5f);
    f->font.scale = scale;
    if (!f->pixels_own) {
        f->font.width = f->pixels * scale > 40 ? 1024 : 512;
        f->font.height = 256;
        f->pixels_own = (unsigned char *)LOGUIA_MALLOC((size_t)f->font.width * (size_t)f->font.height);
    }
    memset(f->pixels_own, 0, (size_t)f->font.width * (size_t)f->font.height);
    f->pixels_own[0] = f->pixels_own[1] = f->pixels_own[f->font.width] = f->pixels_own[f->font.width + 1] = 255;    /* the solid corner */
    f->count = 0; f->full = 0;
    f->pen_x = 4; f->pen_y = 0; f->row_h = 4;
    lg__tt_sync(f);
}
/* a taller picture. What this frame has drawn already points into the picture by fractions of
   its height: those are corrected, so nothing moves. */
static int lg__tt_grow(lg__tt *f) {
    lg__ctx *c = &lg__c;
    int old = f->font.height, i;
    unsigned int k;
    if (old >= 4096) return 0;
    f->pixels_own = (unsigned char *)LOGUIA_REALLOC(f->pixels_own, (size_t)f->font.width * (size_t)old * 2);
    memset(f->pixels_own + (size_t)f->font.width * (size_t)old, 0, (size_t)f->font.width * (size_t)old);
    f->font.height = old * 2;
    if (c->font.source == f)
        for (i = 0; i < c->ncmd; i++) {
            unsigned int lo = 0xFFFFFFFFu, hi = 0;
            if (c->cmds[i].texture) continue;
            for (k = 0; k < c->cmds[i].index_count; k++) {
                unsigned int v = c->idx[c->cmds[i].first_index + k];
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
            for (k = lo; k <= hi && lo <= hi; k++) c->v[k].v *= 0.5f;
        }
    return 1;
}
static const lg_glyph *lg__tt_add(lg__tt *f, unsigned int cp) {
    lg__path p;
    lg_glyph g;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    const lg__tt *src = f;
    int gid = 0, i, w = 0, h = 0, lo = 0, hi = f->count, guard = 0;
    for (; src && guard < 16; src = src->next, guard++) if ((gid = lg__tt_gid(src, cp)) != 0) break;      /* this font, or the ones behind it */
    if (!gid || !src || f->full) return 0;
    memset(&p, 0, sizeof p);
    memset(&g, 0, sizeof g);
    p.k = f->pixels * f->scale / (float)src->units;
    if (src->cff) lg__cff_outline(src, gid, &p);
    else { static const float id[6] = { 1, 0, 0, 1, 0, 0 }; lg__glyf_outline(src, gid, &p, id, 0); }
    for (i = 0; i < p.n; i++) {
        x0 = lg__min(x0, lg__min(p.seg[4 * i], p.seg[4 * i + 2])); x1 = lg__max(x1, lg__max(p.seg[4 * i], p.seg[4 * i + 2]));
        y0 = lg__min(y0, lg__min(p.seg[4 * i + 1], p.seg[4 * i + 3])); y1 = lg__max(y1, lg__max(p.seg[4 * i + 1], p.seg[4 * i + 3]));
    }
    g.cp = cp;
    g.adv = (unsigned short)lg__floor((float)lg__t16(src, src->hmtx + 4 * (unsigned int)(gid < src->hmetrics ? gid : src->hmetrics - 1)) * p.k + 0.5f);
    if (p.n) {
        x0 = lg__floor(x0); y0 = lg__floor(y0);
        w = (int)-lg__floor(-x1) - (int)x0; h = (int)-lg__floor(-y1) - (int)y0;
        if (w > 1000 || h > 1000) w = h = 0;
    }
    if (w > 0 && h > 0) {
        int stride = w + 2, y, x;
        float *a;
        if (f->pen_x + w + 2 > f->font.width) { f->pen_x = 0; f->pen_y += f->row_h; f->row_h = 0; }    /* a new shelf */
        while (f->pen_y + h + 2 > f->font.height)
            if (!lg__tt_grow(f)) { f->full = 1; LOGUIA_FREE(p.seg); return 0; }      /* starts again next frame */
        if (stride * h > f->acc_cap) { f->acc_cap = stride * h; f->acc = (float *)LOGUIA_REALLOC(f->acc, (size_t)f->acc_cap * sizeof(float)); }
        a = f->acc;
        memset(a, 0, (size_t)(stride * h) * sizeof(float));
        for (i = 0; i < p.n; i++)
            lg__raster_line(a, stride, h, p.seg[4 * i] - x0, p.seg[4 * i + 1] - y0, p.seg[4 * i + 2] - x0, p.seg[4 * i + 3] - y0);
        for (y = 0; y < h; y++) {
            float sum = 0;
            unsigned char *out = f->pixels_own + (size_t)(f->pen_y + 1 + y) * (size_t)f->font.width + (size_t)(f->pen_x + 1);
            for (x = 0; x < w; x++) {
                float v;
                sum += a[y * stride + x];
                v = lg__absf(sum);
                out[x] = (unsigned char)(v >= 1 ? 255 : v * 255 + 0.5f);
            }
        }
        g.x = (unsigned short)(f->pen_x + 1); g.y = (unsigned short)(f->pen_y + 1);
        g.w = (unsigned short)w; g.h = (unsigned short)h;
        g.xo = (short)x0; g.yo = (short)(y0 + f->top);
        f->pen_x += w + 2;
        if (h + 2 > f->row_h) f->row_h = h + 2;
    }
    LOGUIA_FREE(p.seg);
    while (lo < hi) { int mid = (lo + hi) / 2; if (f->glyphs[mid].cp < cp) lo = mid + 1; else hi = mid; }     /* the list stays sorted */
    if (f->count == f->cap) { f->cap = f->cap * 2 + 128; f->glyphs = (lg_glyph *)LOGUIA_REALLOC(f->glyphs, (size_t)f->cap * sizeof *f->glyphs); }
    memmove(f->glyphs + lo + 1, f->glyphs + lo, (size_t)(f->count - lo) * sizeof *f->glyphs);
    f->glyphs[lo] = g;
    f->count++;
    lg__tt_sync(f);
    return &f->glyphs[lo];
}

LGDEF lg_font *lg_font_load(const void *data, int size, float pixels) {
    const unsigned char *d = (const unsigned char *)data;
    unsigned int font = 0, head, hhea, maxp, cmap, n, i, len, best = 0, cff, len2;
    int best_rank = 0;
    lg__tt *f;
    if (!d || size < 12 || !(pixels > 0)) return 0;
    f = (lg__tt *)LOGUIA_MALLOC(sizeof *f);
    memset(f, 0, sizeof *f);
    f->data = d; f->size = size;
    if (memcmp(d, "ttcf", 4) == 0) font = lg__t32(f, 12);       /* a collection: its first font */
    head = lg__tt_table(f, font, "head", 0); hhea = lg__tt_table(f, font, "hhea", 0);
    maxp = lg__tt_table(f, font, "maxp", 0); cmap = lg__tt_table(f, font, "cmap", 0);
    f->hmtx = lg__tt_table(f, font, "hmtx", 0);
    f->units = (int)lg__t16(f, head + 18); f->loca_long = lg__ts16(f, head + 50) != 0;
    f->glyphs_total = (int)lg__t16(f, maxp + 4); f->hmetrics = (int)lg__t16(f, hhea + 34);
    f->ascent = lg__ts16(f, hhea + 4); f->descent = lg__ts16(f, hhea + 6); f->gap = lg__ts16(f, hhea + 8);
    n = lg__t16(f, cmap + 2);
    for (i = 0; i < n; i++) {           /* the best map from characters to glyphs: all of Unicode first */
        unsigned int pl = lg__t16(f, cmap + 4 + 8 * i), enc = lg__t16(f, cmap + 6 + 8 * i), at = cmap + lg__t32(f, cmap + 8 + 8 * i);
        int format = (int)lg__t16(f, at), rank = 0;
        if (format == 12 && (pl == 0 || (pl == 3 && enc == 10))) rank = 2;
        else if (format == 4 && (pl == 0 || (pl == 3 && (enc == 1 || enc == 0)))) rank = 1;
        if (rank > best_rank) { best_rank = rank; best = at; f->cmap_format = format; }
    }
    f->cmap = best;
    f->glyf = lg__tt_table(f, font, "glyf", 0); f->loca = lg__tt_table(f, font, "loca", 0);
    if ((cff = lg__tt_table(f, font, "CFF ", &len)) != 0) {
        lg__rd r = lg__rd_make(d + cff, (int)len), top;
        int v[2] = { 0, 0 };
        f->cff = 1;
        f->cfftab = r;
        r.at = r.n > 2 ? r.p[2] : 0;
        lg__cff_index(&r, 1);                                   /* the names */
        top = lg__cff_get(lg__cff_index(&r, 1), 1, 0);          /* the top DICT */
        lg__cff_index(&r, 1);                                   /* the strings */
        f->gsubrs = lg__cff_index(&r, 1);
        if (lg__dict(top, 17, 1, v)) f->charstrings = lg__cff_index_at(f->cfftab, v[0], 1);
        if (lg__dict(top, 0x124, 1, v)) f->fdarray = lg__cff_index_at(f->cfftab, v[0], 1);
        if (lg__dict(top, 0x125, 1, v)) f->fdselect = lg__rd_part(&f->cfftab, v[0], f->cfftab.n - v[0]);
        if (lg__dict(top, 18, 2, v) >= 2) {
            lg__rd priv = lg__rd_part(&f->cfftab, v[1], v[0]);
            int so = 0;
            if (lg__dict(priv, 19, 1, &so)) f->subrs = lg__cff_index_at(f->cfftab, v[1] + so, 1);
        }
    } else if ((cff = lg__tt_table(f, font, "CFF2", &len2)) != 0) {
        lg__rd r = lg__rd_make(d + cff, (int)len2), top;
        int v[2] = { 0, 0 }, hdr = r.n > 4 ? r.p[2] : 0, tl = r.n > 4 ? r.p[3] << 8 | r.p[4] : 0;
        f->cff = 2;
        f->cfftab = r;
        top = lg__rd_part(&r, hdr, tl);
        r.at = hdr + tl;
        f->gsubrs = lg__cff_index(&r, 2);
        if (lg__dict(top, 17, 1, v)) f->charstrings = lg__cff_index_at(f->cfftab, v[0], 2);
        if (lg__dict(top, 0x124, 1, v)) f->fdarray = lg__cff_index_at(f->cfftab, v[0], 2);
        if (lg__dict(top, 0x125, 1, v)) f->fdselect = lg__rd_part(&f->cfftab, v[0], f->cfftab.n - v[0]);
        if (lg__dict(top, 24, 1, v)) f->vstore = lg__rd_part(&f->cfftab, v[0], f->cfftab.n - v[0]);
    }
    if (!head || !hhea || !f->hmtx || !f->cmap || f->units <= 0 || f->hmetrics <= 0 ||
        (f->cff ? !f->charstrings.n : (!f->glyf || !f->loca))) { LOGUIA_FREE(f); return 0; }
    f->pixels = lg__clamp(pixels, 4, 256);
    f->font.source = f;
    lg__tt_reset(f, lg__c.pixel_scale > 0 ? lg__c.pixel_scale : 1);
    return &f->font;
}
#ifndef LOGUIA_NO_STDIO
LGDEF lg_font *lg_font_load_file(const char *path, float pixels) {
    FILE *fp = fopen(path, "rb");
    long n;
    unsigned char *d;
    lg_font *font = 0;
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
    d = n > 0 && n < 0x7FFFFFFF ? (unsigned char *)LOGUIA_MALLOC((size_t)n) : 0;
    if (d && fread(d, 1, (size_t)n, fp) == (size_t)n) font = lg_font_load(d, (int)n, pixels);
    fclose(fp);
    if (font) ((lg__tt *)font->source)->own_data = d;
    else LOGUIA_FREE(d);
    return font;
}
#endif
LGDEF void lg_font_fallback(lg_font *font, lg_font *other) {
    lg__tt *f = font ? (lg__tt *)font->source : 0;
    if (!f || (other && other->source == f)) return;
    f->next = other ? (lg__tt *)other->source : 0;
    lg__tt_reset(f, f->scale);          /* characters drawn before may come out differently now */
}
LGDEF void lg_font_free(lg_font *font) {
    lg__tt *f = font ? (lg__tt *)font->source : 0;
    if (!f) return;
    if (lg__c.font.source == f) {       /* it was in use: back to the built-in one */
#ifndef LOGUIA_NO_DEFAULT_FONT
        lg__default_font(lg__c.pixel_scale >= 1.25f);
#else
        memset(&lg__c.font, 0, sizeof lg__c.font);
#endif
    }
    LOGUIA_FREE(f->pixels_own); LOGUIA_FREE(f->glyphs); LOGUIA_FREE(f->acc); LOGUIA_FREE(f->own_data);
    LOGUIA_FREE(f);
}

LGDEF void lg_set_font(const lg_font *font) {
    lg__c.font = *font;
    if (font->source && ((lg__tt *)font->source)->scale != (lg__c.pixel_scale > 0 ? lg__c.pixel_scale : 1))
        lg__tt_reset((lg__tt *)font->source, lg__c.pixel_scale);
}
LGDEF const lg_font *lg_get_font(void) { return &lg__c.font; }
LGDEF void lg_set_pixel_scale(float scale) {
    lg__ctx *c = &lg__c;
    c->pixel_scale = scale > 0 ? scale : 1;
#ifndef LOGUIA_NO_DEFAULT_FONT
    if (c->font.pixels && (c->font.pixels == c->own_pixels || c->font.pixels == c->own_pixels2))
        lg__default_font(c->pixel_scale >= 1.25f);
#endif
    if (c->font.source && ((lg__tt *)c->font.source)->scale != c->pixel_scale) lg__tt_reset((lg__tt *)c->font.source, c->pixel_scale);
}
/* font picture pixels to interface units */
static float lg__fs(void) { return lg__c.style.font_scale / (lg__c.font.scale > 0 ? lg__c.font.scale : 1); }

static const lg_glyph *lg__find_glyph(unsigned int cp) {
    int lo = 0, hi = lg__c.font.glyph_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        unsigned int m = lg__c.font.glyphs[mid].cp;
        if (m == cp) return &lg__c.font.glyphs[mid];
        if (m < cp) lo = mid + 1; else hi = mid - 1;
    }
    return lg__c.font.source ? lg__tt_add((lg__tt *)lg__c.font.source, cp) : 0;     /* a loaded font draws it now */
}
static const lg_glyph *lg__glyph(unsigned int cp) {
    const lg_glyph *g = lg__find_glyph(cp == '\t' ? ' ' : cp);
    return g ? g : lg__find_glyph('?');
}
/* how far a character moves the pen, in font pixels; a tab counts as four spaces */
static float lg__adv(const lg_glyph *g, unsigned int cp) { return g ? (float)g->adv * (cp == '\t' ? 4 : 1) : 0; }

/* reads one UTF-8 character, returns where the next one starts */
static const char *lg__utf8(const char *s, const char *end, unsigned int *cp) {
    unsigned int c = (unsigned char)*s++;
    int more = c < 0x80 ? 0 : c < 0xC0 ? -1 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : c < 0xF8 ? 3 : -1;
    if (more < 0) { *cp = 0xFFFD; return s; }
    if (more) c &= 0x3Fu >> more;
    while (more-- > 0) {
        if (s >= end || ((unsigned char)*s & 0xC0) != 0x80) { *cp = 0xFFFD; return s; }
        c = (c << 6) | ((unsigned char)*s++ & 0x3F);
    }
    *cp = c;
    return s;
}

/* writes one character as UTF-8, returns how many bytes */
static int lg__utf8_put(unsigned int cp, char *u) {
    int n = 0;
    if (cp < 0x80) u[n++] = (char)cp;
    else if (cp < 0x800) { u[n++] = (char)(0xC0 | (cp >> 6)); u[n++] = (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { u[n++] = (char)(0xE0 | (cp >> 12)); u[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[n++] = (char)(0x80 | (cp & 0x3F)); }
    else { u[n++] = (char)(0xF0 | (cp >> 18)); u[n++] = (char)(0x80 | ((cp >> 12) & 0x3F)); u[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[n++] = (char)(0x80 | (cp & 0x3F)); }
    return n;
}

static float lg__text_w(const char *s, const char *end) {
    float w = 0;
    unsigned int cp;
    while (s < end) {
        const lg_glyph *g;
        s = lg__utf8(s, end, &cp);
        g = lg__glyph(cp);
        w += lg__adv(g, cp);
    }
    return w * lg__fs();
}
static float lg__line_h(void) { return lg__floor(lg__c.font.line_height * lg__fs() + 0.5f); }
LGDEF float lg_text_width(const char *text) { return lg__text_w(text, text + strlen(text)); }
LGDEF float lg_line_height(void) { return lg__line_h(); }

/* ---------------------------------------------------------------- drawing */

static lg__rect lg__clip(void) {
    return lg__c.nclips ? lg__c.clips[lg__c.nclips - 1] : lg__r(0, 0, lg__c.w, lg__c.h);
}
static void lg__push_clip(lg__rect r) {
    lg__rect c = lg__clip();
    float x0 = lg__max(r.x, c.x), y0 = lg__max(r.y, c.y);
    float x1 = lg__min(r.x + r.w, c.x + c.w), y1 = lg__min(r.y + r.h, c.y + c.h);
    LOGUIA_ASSERT(lg__c.nclips < LOGUIA_CLIP_STACK);
    lg__c.clips[lg__c.nclips++] = lg__r(x0, y0, lg__max(0, x1 - x0), lg__max(0, y1 - y0));
}
static void lg__pop_clip(void) { if (lg__c.nclips) lg__c.nclips--; }
/* a clip rectangle that ignores the ones around it: popups and tooltips are not inside anything */
static void lg__push_clip_raw(lg__rect r) {
    LOGUIA_ASSERT(lg__c.nclips < LOGUIA_CLIP_STACK);
    lg__c.clips[lg__c.nclips++] = r;
}

/* makes room for nv vertices and ni indices in a command that matches the current state */
static lg_vertex *lg__prim(int nv, int ni, void *tex, unsigned int **idx, unsigned int *base) {
    lg__ctx *c = &lg__c;
    lg__rect cl = lg__clip();
    lg_cmd *cmd = c->ncmd ? &c->cmds[c->ncmd - 1] : 0;
    if (c->nv + nv > c->capv) {
        c->capv = (c->nv + nv) * 2 + 1024;
        c->v = (lg_vertex *)LOGUIA_REALLOC(c->v, (size_t)c->capv * sizeof *c->v);
    }
    if (c->ni + ni > c->capi) {
        c->capi = (c->ni + ni) * 2 + 2048;
        c->idx = (unsigned int *)LOGUIA_REALLOC(c->idx, (size_t)c->capi * sizeof *c->idx);
    }
    if (!cmd || cmd->texture != tex || c->cmd_layer[c->ncmd - 1] != c->layer || cmd->clip_x != cl.x ||
        cmd->clip_y != cl.y || cmd->clip_w != cl.w || cmd->clip_h != cl.h) {
        if (c->ncmd == c->capcmd) {
            c->capcmd = c->capcmd * 2 + 64;
            c->cmds = (lg_cmd *)LOGUIA_REALLOC(c->cmds, (size_t)c->capcmd * sizeof *c->cmds);
            c->cmd_layer = (int *)LOGUIA_REALLOC(c->cmd_layer, (size_t)c->capcmd * sizeof *c->cmd_layer);
        }
        cmd = &c->cmds[c->ncmd];
        cmd->clip_x = cl.x; cmd->clip_y = cl.y; cmd->clip_w = cl.w; cmd->clip_h = cl.h;
        cmd->texture = tex; cmd->first_index = (unsigned int)c->ni; cmd->index_count = 0;
        c->cmd_layer[c->ncmd++] = c->layer;
    }
    cmd->index_count += (unsigned int)ni;
    *idx = c->idx + c->ni;
    *base = (unsigned int)c->nv;
    c->ni += ni;
    c->nv += nv;
    return c->v + c->nv - nv;
}
static void lg__vert(lg_vertex *v, float x, float y, float u, float w, unsigned int col) {
    v->x = x; v->y = y; v->u = u; v->v = w;
    v->r = (unsigned char)(col >> 24); v->g = (unsigned char)(col >> 16);
    v->b = (unsigned char)(col >> 8);  v->a = (unsigned char)col;
}
static void lg__quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, unsigned int col, void *tex) {
    lg__rect cl = lg__clip();
    lg_vertex *v;
    unsigned int *i, b;
    if (w <= 0 || h <= 0 || !(col & 255)) return;
    if (x >= cl.x + cl.w || y >= cl.y + cl.h || x + w <= cl.x || y + h <= cl.y) return;    /* not visible */
    v = lg__prim(4, 6, tex, &i, &b);
    lg__vert(v, x, y, u0, v0, col);         lg__vert(v + 1, x + w, y, u1, v0, col);
    lg__vert(v + 2, x + w, y + h, u1, v1, col); lg__vert(v + 3, x, y + h, u0, v1, col);
    i[0] = b; i[1] = b + 1; i[2] = b + 2; i[3] = b; i[4] = b + 2; i[5] = b + 3;
}
/* the solid white corner of the font picture */
static float lg__wu(void) { return 1.0f / (float)lg__c.font.width; }
static float lg__wv(void) { return 1.0f / (float)lg__c.font.height; }

static void lg__fill(lg__rect r, unsigned int col) {
    lg__quad(r.x, r.y, r.w, r.h, lg__wu(), lg__wv(), lg__wu(), lg__wv(), col, 0);
}
static void lg__outline(lg__rect r, float t, unsigned int col) {
    if (t <= 0) return;
    lg__fill(lg__r(r.x, r.y, r.w, t), col);
    lg__fill(lg__r(r.x, r.y + r.h - t, r.w, t), col);
    lg__fill(lg__r(r.x, r.y + t, t, r.h - 2 * t), col);
    lg__fill(lg__r(r.x + r.w - t, r.y + t, t, r.h - 2 * t), col);
}
static void lg__tri(float x0, float y0, float x1, float y1, float x2, float y2, unsigned int col) {
    unsigned int *i, b;
    lg_vertex *v;
    if (!(col & 255)) return;
    v = lg__prim(3, 3, 0, &i, &b);
    lg__vert(v, x0, y0, lg__wu(), lg__wv(), col);
    lg__vert(v + 1, x1, y1, lg__wu(), lg__wv(), col);
    lg__vert(v + 2, x2, y2, lg__wu(), lg__wv(), col);
    i[0] = b; i[1] = b + 1; i[2] = b + 2;
}
/* Shapes with smooth edges. A path is a closed list of points (x, y, x, y, ...) going clockwise.
   Every edge gets a thin see-through rim, so curves do not look like stairs. */
#define LG__PATH_MAX 28

/* the outline of a rectangle with round corners; corners: 1 top-left, 2 top-right, 4 bottom-right, 8 bottom-left */
static int lg__round_path(lg__rect r, float rad, int corners, float *p) {
    static const float cs[7] = { 1, 0.9659258f, 0.8660254f, 0.7071068f, 0.5f, 0.2588190f, 0 };    /* cos 0..90 degrees */
    float big = rad * lg__c.pixel_scale;
    int n = 0, k, i, step = big < 3 ? 3 : big < 7 ? 2 : 1;
    for (k = 0; k < 4; k++) {
        float cr = (corners >> k & 1) ? rad : 0;
        float cx = (k == 0 || k == 3) ? r.x + cr : r.x + r.w - cr, cy = k < 2 ? r.y + cr : r.y + r.h - cr;
        for (i = 0; i <= 6; i += step) {
            float co = cs[i] * cr, si = cs[6 - i] * cr, x, y;
            if (k == 0) { x = cx - co; y = cy - si; } else if (k == 1) { x = cx + si; y = cy - co; }
            else if (k == 2) { x = cx + co; y = cy + si; } else { x = cx - si; y = cy + co; }
            if (!n || p[2 * n - 2] != x || p[2 * n - 1] != y) { p[2 * n] = x; p[2 * n + 1] = y; n++; }
            if (cr <= 0) break;
        }
    }
    if (n > 1 && p[0] == p[2 * n - 2] && p[1] == p[2 * n - 1]) n--;
    return n;
}
/* for every point: the direction out of the shape, sized so that moving the point by it shifts both its edges by 1 */
static void lg__path_out(const float *p, int n, float *o) {
    float e[2 * LG__PATH_MAX];
    int i;
    for (i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float dx = p[2 * j] - p[2 * i], dy = p[2 * j + 1] - p[2 * i + 1], len = lg__sqrt(dx * dx + dy * dy);
        e[2 * i] = len > 0 ? dy / len : 0; e[2 * i + 1] = len > 0 ? -dx / len : 0;
    }
    for (i = 0; i < n; i++) {
        int a = (i + n - 1) % n;
        float mx = (e[2 * a] + e[2 * i]) * 0.5f, my = (e[2 * a + 1] + e[2 * i + 1]) * 0.5f, d2 = mx * mx + my * my;
        if (d2 > 0.000001f) { float inv = 1 / d2; if (inv > 100) inv = 100; mx *= inv; my *= inv; }
        o[2 * i] = mx; o[2 * i + 1] = my;
    }
}
static void lg__poly_fill(const float *p, int n, unsigned int col) {
    float o[2 * LG__PATH_MAX], aa = 0.5f / lg__c.pixel_scale, wu = lg__wu(), wv = lg__wv();
    unsigned int *idx, base;
    lg_vertex *v;
    int i;
    if (n < 3 || !(col & 255)) return;
    lg__path_out(p, n, o);
    v = lg__prim(2 * n, (n - 2) * 3 + n * 6, 0, &idx, &base);
    for (i = 0; i < n; i++) {
        lg__vert(v + 2 * i, p[2 * i] - o[2 * i] * aa, p[2 * i + 1] - o[2 * i + 1] * aa, wu, wv, col);
        lg__vert(v + 2 * i + 1, p[2 * i] + o[2 * i] * aa, p[2 * i + 1] + o[2 * i + 1] * aa, wu, wv, col & 0xFFFFFF00u);
    }
    for (i = 2; i < n; i++) { *idx++ = base; *idx++ = base + 2 * (unsigned int)(i - 1); *idx++ = base + 2 * (unsigned int)i; }
    for (i = 0; i < n; i++) {
        unsigned int a = base + 2 * (unsigned int)i, b = base + 2 * (unsigned int)((i + 1) % n);
        *idx++ = a; *idx++ = a + 1; *idx++ = b + 1; *idx++ = a; *idx++ = b + 1; *idx++ = b;
    }
}
/* a line of thickness t along the inside of the path */
static void lg__poly_line(const float *p, int n, float t, unsigned int col) {
    float o[2 * LG__PATH_MAX], off[4], aa = 0.5f / lg__c.pixel_scale, wu = lg__wu(), wv = lg__wv();
    unsigned int *idx, base;
    lg_vertex *v;
    int i, k;
    if (n < 3 || t <= 0 || !(col & 255)) return;
    lg__path_out(p, n, o);
    off[0] = aa; off[1] = -aa; off[2] = -lg__max(t - aa, aa); off[3] = off[2] - 2 * aa;
    v = lg__prim(4 * n, n * 18, 0, &idx, &base);
    for (i = 0; i < n; i++)
        for (k = 0; k < 4; k++)
            lg__vert(v + 4 * i + k, p[2 * i] + o[2 * i] * off[k], p[2 * i + 1] + o[2 * i + 1] * off[k], wu, wv,
                     k == 0 || k == 3 ? col & 0xFFFFFF00u : col);
    for (i = 0; i < n; i++) {
        unsigned int a = base + 4 * (unsigned int)i, b = base + 4 * (unsigned int)((i + 1) % n);
        for (k = 0; k < 3; k++, a++, b++) { *idx++ = a; *idx++ = b; *idx++ = b + 1; *idx++ = a; *idx++ = b + 1; *idx++ = a + 1; }
    }
}
static int lg__offscreen(lg__rect r) {
    lg__rect cl = lg__clip();
    return r.w <= 0 || r.h <= 0 || r.x >= cl.x + cl.w || r.y >= cl.y + cl.h || r.x + r.w <= cl.x || r.y + r.h <= cl.y;
}
/* a filled box and an outlined box with round corners (see lg__round_path for corners) */
static void lg__fill_round(lg__rect r, unsigned int col, float rad, int corners) {
    float p[2 * LG__PATH_MAX];
    rad = lg__min(rad, lg__min(r.w, r.h) / 2);
    if (rad < 0.5f || !corners) { lg__fill(r, col); return; }
    if (!lg__offscreen(r)) lg__poly_fill(p, lg__round_path(r, rad, corners, p), col);
}
static void lg__outline_round(lg__rect r, float t, unsigned int col, float rad, int corners) {
    float p[2 * LG__PATH_MAX];
    rad = lg__min(rad, lg__min(r.w, r.h) / 2);
    if (rad < 0.5f || !corners) { lg__outline(r, t, col); return; }
    if (!lg__offscreen(r)) lg__poly_line(p, lg__round_path(r, rad, corners, p), t, col);
}
/* the usual case: all four corners, as round as the style says */
static void lg__fillr(lg__rect r, unsigned int col) { lg__fill_round(r, col, lg__c.style.rounding, 15); }
static void lg__outliner(lg__rect r, float t, unsigned int col) { lg__outline_round(r, t, col, lg__c.style.rounding, 15); }

static void lg__glyph_quad(const lg_glyph *g, float x, float y, unsigned int col) {
    float sc = lg__fs(), fw = (float)lg__c.font.width, fh = (float)lg__c.font.height;
    if (!g->w) return;
    lg__quad(x + g->xo * sc, y + g->yo * sc, g->w * sc, g->h * sc,
             g->x / fw, g->y / fh, (g->x + g->w) / fw, (g->y + g->h) / fh, col, 0);
}
static void lg__text(float x, float y, const char *s, const char *end, unsigned int col) {
    lg__rect cl = lg__clip();
    unsigned int cp;
    float sc = lg__fs(), ps = lg__c.pixel_scale;
    x = lg__floor(x * ps) / ps; y = lg__floor(y * ps) / ps;         /* on whole screen pixels, so it stays sharp */
    if (y >= cl.y + cl.h || y + lg__line_h() <= cl.y) return;
    while (s < end && x < cl.x + cl.w) {
        const lg_glyph *g;
        s = lg__utf8(s, end, &cp);
        g = lg__glyph(cp);
        if (!g) continue;
        if (cp != '\t' && x + g->adv * sc > cl.x) lg__glyph_quad(g, x, y, col);
        x += lg__adv(g, cp) * sc;
    }
}
/* an arrow or a check mark inside the square (x, y, size): from the font when it has one */
static void lg__icon(unsigned int cp, float x, float y, float size, unsigned int col) {
    const lg_glyph *g = lg__find_glyph(cp);
    float sc = lg__fs(), m = lg__floor(size * 0.3f);
    if (g) {
        lg__glyph_quad(g, lg__floor(x + (size - g->w * sc) / 2 - g->xo * sc),
                       lg__floor(y + (size - g->h * sc) / 2 - g->yo * sc), col);
    } else if (cp == 0x25B6) {
        lg__tri(x + m, y + m, x + size - m, y + size / 2, x + m, y + size - m, col);
    } else if (cp == 0x25BC) {
        lg__tri(x + m, y + m, x + size - m, y + m, x + size / 2, y + size - m, col);
    } else if (cp == 0x25B2) {
        lg__tri(x + m, y + size - m, x + size / 2, y + m, x + size - m, y + size - m, col);
    } else {
        lg__fill(lg__r(x + m, y + m, size - 2 * m, size - 2 * m), col);
    }
}

LGDEF void lg_draw_rect(float x, float y, float w, float h, unsigned int rgba) { lg__fill(lg__r(x, y, w, h), rgba); }
LGDEF void lg_draw_rounded(float x, float y, float w, float h, float radius, unsigned int rgba) { lg__fill_round(lg__r(x, y, w, h), rgba, radius, 15); }
LGDEF void lg_draw_outline(float x, float y, float w, float h, float t, unsigned int rgba) { lg__outline(lg__r(x, y, w, h), t, rgba); }
LGDEF void lg_draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, unsigned int rgba) {
    lg__tri(x0, y0, x1, y1, x2, y2, rgba);
}
LGDEF void lg_draw_line(float x0, float y0, float x1, float y1, float t, unsigned int rgba) {
    float dx = x1 - x0, dy = y1 - y0, len = lg__sqrt(dx * dx + dy * dy), nx, ny;
    if (len <= 0) return;
    nx = -dy / len * t * 0.5f; ny = dx / len * t * 0.5f;
    lg__tri(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, rgba);
    lg__tri(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, rgba);
}
LGDEF void lg_draw_text(float x, float y, const char *text, unsigned int rgba) { lg__text(x, y, text, text + strlen(text), rgba); }
LGDEF void lg_draw_image(void *texture, float x, float y, float w, float h, unsigned int rgba) {
    lg__quad(x, y, w, h, 0, 0, 1, 1, rgba, texture);
}
LGDEF void lg_mouse(float *x, float *y) { if (x) *x = lg__c.mx; if (y) *y = lg__c.my; }
LGDEF int lg_mouse_pressed(int button) { return button >= 0 && button < 3 && lg__c.mpressed[button]; }

/* ---------------------------------------------------------------- docking */

#define LG__SPLIT 4         /* the gap between two docked windows; it can be dragged */
#define LG__GRIP 10         /* the strip along the top of a floating group; it moves the group */

/* a window that is on screen (it was shown this frame or the one before) */
static int lg__box_live(const lg__box *b) { return b->id && b->is_window && !b->is_popup && b->last_frame >= lg__c.frame - 1; }
/* the top of the tree that place n is in: 0 for the screen, something else for a floating group */
static int lg__dock_root(int n) {
    int guard = 0;
    while (lg__c.docks[n].parent >= 0 && guard++ < LOGUIA_MAX_DOCKS) n = lg__c.docks[n].parent;
    return n;
}
/* How far up things are: the one with the biggest number is drawn last and gets the mouse.
   A window that fills the screen 0, docked windows 1, floating windows and groups by their z;
   a tab that is behind another sits one above its own group, so its title can be clicked. */
static int lg__dock_level(int n) { n = lg__dock_root(n); return n ? (lg__c.docks[n].z + 3) * 2 : 1; }
static int lg__level(const lg__box *b) {
    if (b->is_popup) return b->z;
    if (b->back == 1) return 0;
    if (b->back) return (b->dock ? lg__dock_level(b->dock - 1) : 1) + b->hidden;
    return (b->z + 3) * 2;
}
/* the mouse has moved more than d since the button went down */
static int lg__far(float d) {
    float dx = lg__c.mx - lg__c.press_x, dy = lg__c.my - lg__c.press_y;
    return dx * dx + dy * dy > d * d;
}

static void lg__dock_place(int n, lg__rect r) {
    lg__dock *d = &lg__c.docks[n];
    d->rect = r;
    if (d->kind >= LG__D_COLS) {
        lg__dock *a = &lg__c.docks[d->a], *b = &lg__c.docks[d->b];
        int cols = d->kind == LG__D_COLS;
        float size = (cols ? r.w : r.h) - LG__SPLIT, lim = lg__min(60, size / 2), first = lg__floor(lg__clamp(size * d->share, lim, size - lim));
        if (!a->live || !b->live || size <= 0) {        /* only one half is in use: it gets everything */
            lg__dock_place(d->a, a->live ? r : lg__r(r.x, r.y, 0, 0));
            lg__dock_place(d->b, b->live && !a->live ? r : lg__r(r.x, r.y, 0, 0));
        } else if (cols) {
            lg__dock_place(d->a, lg__r(r.x, r.y, first, r.h));
            lg__dock_place(d->b, lg__r(r.x + first + LG__SPLIT, r.y, size - first, r.h));
        } else {
            lg__dock_place(d->a, lg__r(r.x, r.y, r.w, first));
            lg__dock_place(d->b, lg__r(r.x, r.y + first + LG__SPLIT, r.w, size - first));
        }
    }
}
/* place n has lost its last window: the other half of the split takes its parent's place */
static void lg__dock_close(int n) {
    lg__ctx *c = &lg__c;
    int pi = c->docks[n].parent, si = c->docks[pi].a == n ? c->docks[pi].b : c->docks[pi].a, i;
    lg__dock *p = &c->docks[pi], *s = &c->docks[si];
    int up = p->parent, z = p->z;
    lg__rect r = p->rect, fr = p->fr;
    *p = *s;
    p->parent = up; p->rect = r; p->fr = fr; p->z = z;
    if (p->kind >= LG__D_COLS) c->docks[p->a].parent = c->docks[p->b].parent = pi;
    for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].dock == si + 1) c->boxes[i].dock = pi + 1;
    memset(s, 0, sizeof *s);
    memset(&c->docks[n], 0, sizeof c->docks[n]);
}
/* tidies the tree and gives every place its rectangle */
static void lg__undock(lg__box *b);
static void lg__dock_layout(void) {
    lg__ctx *c = &lg__c;
    float top = c->bar_frame >= c->frame - 1 ? c->bar_h : 0;
    int n, i, p;
    for (n = 0; n < LOGUIA_MAX_DOCKS; n++) {
        lg__dock *d = &c->docks[n];
        lg__box *only = 0;
        int count = 0;
        if (d->kind != LG__D_LEAF) continue;
        for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].id && c->boxes[i].is_window && c->boxes[i].dock == n + 1) { only = &c->boxes[i]; count++; }
        if (n && d->parent < 0 && count < 2) {      /* a floating group of one window is just that window again */
            if (only) { lg__undock(only); only->rect = d->fr; only->z = ++c->top_z; }
            memset(d, 0, sizeof *d);
        } else if (count) continue;
        else if (d->centre || d->parent < 0) { d->kind = LG__D_HOLE; d->centre = 0; d->tab = d->want = 0; }
        else { lg__dock_close(n); n = -1; }         /* the tree changed: look again from the start */
    }
    for (n = 0; n < LOGUIA_MAX_DOCKS; n++) c->docks[n].live = c->docks[n].kind == LG__D_HOLE;
    for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].dock && lg__box_live(&c->boxes[i])) c->docks[c->boxes[i].dock - 1].live = 1;
    for (n = 0; n < LOGUIA_MAX_DOCKS; n++) {
        lg__dock *d = &c->docks[n];
        lg__box *first = 0;
        int found = 0;
        if (!d->live || d->kind >= LG__D_COLS) continue;
        for (p = d->parent; p >= 0; p = c->docks[p].parent) c->docks[p].live = 1;
        if (d->kind != LG__D_LEAF) continue;
        for (i = 0; i < LOGUIA_MAX_BOXES; i++) {     /* the tab in front has to be a window that is there */
            lg__box *b = &c->boxes[i];
            if (b->dock != n + 1 || !lg__box_live(b)) continue;
            if (b->id == d->tab) found = 1;
            if (!first || b->order < first->order) first = b;
        }
        if (!found && first) d->tab = first->id;
    }
    lg__dock_place(0, lg__r(0, top, c->w, lg__max(0, c->h - top)));
    for (n = 1; n < LOGUIA_MAX_DOCKS; n++) {        /* the groups that float */
        lg__dock *d = &c->docks[n];
        if (d->kind == LG__D_FREE || d->parent >= 0) continue;
        d->fr.w = lg__max(d->fr.w, 160); d->fr.h = lg__max(d->fr.h, 120);
        d->fr.x = lg__floor(lg__clamp(d->fr.x, 40 - d->fr.w, lg__max(0, c->w - 40)));
        d->fr.y = lg__floor(lg__clamp(d->fr.y, top, lg__max(top, c->h - 20)));
        lg__dock_place(n, lg__r(d->fr.x, d->fr.y + LG__GRIP, d->fr.w, d->fr.h - LG__GRIP));
    }
}
/* the part of r that a window docked at that side gets */
static lg__rect lg__dock_part(lg__rect r, int side, float share) {
    if (side == LG_DOCK_LEFT) return lg__r(r.x, r.y, r.w * share, r.h);
    if (side == LG_DOCK_RIGHT) return lg__r(r.x + r.w * (1 - share), r.y, r.w * share, r.h);
    if (side == LG_DOCK_TOP) return lg__r(r.x, r.y, r.w, r.h * share);
    if (side == LG_DOCK_BOTTOM) return lg__r(r.x, r.y + r.h * (1 - share), r.w, r.h * share);
    return r;
}
/* a window takes half of another window's place, or a quarter of anything bigger */
static float lg__dock_share(int n) { return lg__c.docks[n].kind == LG__D_LEAF ? 0.5f : 0.25f; }
static int lg__dock_new(void) {
    int n;
    for (n = 1; n < LOGUIA_MAX_DOCKS; n++) if (lg__c.docks[n].kind == LG__D_FREE) return n;
    return 0;
}
static void lg__undock(lg__box *b) {
    b->dock = 0; b->hidden = 0;
    b->rect.w = b->fw > 0 ? b->fw : 320; b->rect.h = b->fh > 0 ? b->fh : 240;
}
/* docks the floating window b: at a side of place n (which is split for it), or into n as a tab */
static void lg__dock_put(lg__box *b, int n, int side, float share) {
    lg__ctx *c = &lg__c;
    lg__dock *d = &c->docks[n];
    int xi, yi, i, first = side == LG_DOCK_LEFT || side == LG_DOCK_TOP;
    if (d->kind == LG__D_FREE || b->dock) return;
    if (side == LG_DOCK_TAB) {
        if (d->kind == LG__D_HOLE) { d->kind = LG__D_LEAF; d->centre = 1; }
        if (d->kind != LG__D_LEAF) return;
        d->tab = b->id; d->want = 0;
    } else {
        if (share <= 0) share = lg__dock_share(n);
        xi = lg__dock_new();
        if (!xi) return;
        c->docks[xi] = *d;                      /* what was here moves into one half... */
        c->docks[xi].parent = n;
        yi = lg__dock_new();
        if (!yi) { c->docks[xi].kind = LG__D_FREE; return; }
        if (d->kind >= LG__D_COLS) c->docks[d->a].parent = c->docks[d->b].parent = xi;
        for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].dock == n + 1) c->boxes[i].dock = xi + 1;
        memset(&c->docks[yi], 0, sizeof c->docks[yi]);          /* ...and the window gets the other */
        c->docks[yi].kind = LG__D_LEAF; c->docks[yi].parent = n; c->docks[yi].tab = b->id;
        d->kind = side <= LG_DOCK_RIGHT ? LG__D_COLS : LG__D_ROWS;
        d->a = first ? yi : xi; d->b = first ? xi : yi;
        share = lg__clamp(share, 0.05f, 0.95f);
        d->share = first ? share : 1 - share;
        d->tab = d->want = 0; d->centre = 0;
        n = yi;
    }
    if (b->placed) { b->fw = b->rect.w; b->fh = b->rect.h; }
    b->dock = n + 1;
    b->order = ++c->dock_order;
}
/* two floating windows become a group: a new tree that floats where o was, with o in it. 0 if there is no room for one */
static int lg__dock_group(lg__box *o) {
    lg__ctx *c = &lg__c;
    int n = o->dock ? 0 : lg__dock_new();
    if (!n) return 0;
    memset(&c->docks[n], 0, sizeof c->docks[n]);
    c->docks[n].kind = LG__D_LEAF; c->docks[n].parent = -1;
    c->docks[n].fr = o->rect; c->docks[n].z = ++c->top_z;
    lg__dock_put(o, n, LG_DOCK_TAB, 0);
    return n;
}
/* at the start of a frame: what the mouse did to docked windows in the frame before */
static void lg__dock_frame(void) {
    lg__ctx *c = &lg__c;
    int n, i;
    for (n = 0; n < LOGUIA_MAX_DOCKS; n++) if (c->docks[n].want) { c->docks[n].tab = c->docks[n].want; c->docks[n].want = 0; }
    if (c->drop_side) {
        lg__box *b = 0;
        for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].id == c->drop_id && c->boxes[i].is_window) b = &c->boxes[i];
        if (b && c->drop_side < 0 && b->dock) {         /* pulled out: it floats, still held by the mouse */
            lg__undock(b);
            c->grab_x = lg__min(c->grab_x, b->rect.w - 30);
            b->rect.x = c->mx - c->grab_x; b->rect.y = c->my - c->grab_y;
            b->z = ++c->top_z;
        } else if (b && c->drop_side > 0 && c->drop_node >= 0) lg__dock_put(b, c->drop_node, c->drop_side, 0);
        else if (b && c->drop_side > 0 && !b->dock) {      /* onto a floating window */
            lg__box *o = 0;
            for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].id == c->drop_other && c->boxes[i].is_window && &c->boxes[i] != b) o = &c->boxes[i];
            n = o ? lg__dock_group(o) : 0;
            if (n) lg__dock_put(b, n, c->drop_side, 0);
        }
        c->drop_side = 0;
    }
    lg__dock_layout();
}
/* the mouse is on r, and no window further up than level is in the way */
static int lg__dock_over(lg__rect r, int level) {
    lg__ctx *c = &lg__c;
    return !c->active && lg__in(r, c->mx, c->my) && (!c->hover_win || lg__level(c->hover_win) <= level);
}
/* the gaps between docked windows: dragging one moves the border. And the floating groups: their strip and corner */
static void lg__dock_splits(void) {
    lg__ctx *c = &lg__c;
    int n, pass;
    for (n = 1; n < LOGUIA_MAX_DOCKS; n++) {
        lg__dock *d = &c->docks[n];
        unsigned int id = lg__hash(&n, (int)sizeof n, 0x6B0A7ED1u), id2 = lg__hash(&n, (int)sizeof n, 0x6B0A7ED2u);
        int level = lg__dock_level(n), over, over2;
        if (d->kind == LG__D_FREE || d->parent >= 0 || !d->live) continue;
        over = lg__dock_over(lg__r(d->fr.x, d->fr.y, d->fr.w, LG__GRIP), level + 1);
        over2 = !over && lg__dock_over(lg__r(d->fr.x + d->fr.w - 14, d->fr.y + d->fr.h - 14, 14, 14), level + 1);
        if (over && c->mpressed[0]) { c->active = id; c->grab_x = c->mx - d->fr.x; c->grab_y = c->my - d->fr.y; d->z = ++c->top_z; }
        if (over2 && c->mpressed[0]) { c->active = id2; c->grab_x = d->fr.x + d->fr.w - c->mx; c->grab_y = d->fr.y + d->fr.h - c->my; d->z = ++c->top_z; }
        if (c->active == id) { d->fr.x = c->mx - c->grab_x; d->fr.y = c->my - c->grab_y; lg__dock_layout(); }
        if (c->active == id2) { d->fr.w = lg__floor(c->mx + c->grab_x - d->fr.x); d->fr.h = lg__floor(c->my + c->grab_y - d->fr.y); lg__dock_layout(); }
        c->layer = lg__dock_level(n);
        lg__fill(lg__r(d->fr.x, d->fr.y, d->fr.w, LG__GRIP), lg__col(c->active == id || over ? LG_COL_BUTTON_HOVER : LG_COL_BUTTON));
        c->layer++;                                 /* the corner lies over the group's windows */
        lg__tri(d->fr.x + d->fr.w - 2, d->fr.y + d->fr.h - 12, d->fr.x + d->fr.w - 2, d->fr.y + d->fr.h - 2, d->fr.x + d->fr.w - 12, d->fr.y + d->fr.h - 2,
                lg__col(c->active == id2 || over2 ? LG_COL_ACCENT : LG_COL_BORDER));
    }
    for (n = 0; n < LOGUIA_MAX_DOCKS; n++) {
        lg__dock *d = &c->docks[n];
        unsigned int id = lg__hash(&n, (int)sizeof n, 0x5EED5EEDu);
        int cols = d->kind == LG__D_COLS, over = 0;
        lg__rect a, gap;
        if (d->kind < LG__D_COLS || !c->docks[d->a].live || !c->docks[d->b].live) continue;
        gap = lg__r(0, 0, 0, 0);
        for (pass = 0; pass < 2; pass++) {
            a = c->docks[d->a].rect;
            gap = cols ? lg__r(a.x + a.w, a.y, LG__SPLIT, a.h) : lg__r(a.x, a.y + a.h, a.w, LG__SPLIT);
            if (pass) break;
            over = lg__dock_over(lg__r(gap.x - 2, gap.y - 2, gap.w + 4, gap.h + 4), lg__dock_level(n) + 1);
            if (over && c->mpressed[0]) { c->active = id; c->grab_x = cols ? c->mx - gap.x : c->my - gap.y; }
            if (c->active != id) break;
            else {
                float size = (cols ? d->rect.w : d->rect.h) - LG__SPLIT;
                if (size > 0) d->share = lg__clamp(((cols ? c->mx - d->rect.x : c->my - d->rect.y) - c->grab_x) / size, 0.02f, 0.98f);
                lg__dock_layout();
            }
        }
        c->layer = lg__dock_level(n);
        lg__fill(gap, lg__col(c->active == id || over ? LG_COL_ACCENT : LG_COL_BORDER));
    }
    c->layer = 0;
}
/* while the window b is dragged: the squares it can be dropped on, and where it would land */
static void lg__dock_targets(lg__box *b) {
    static const float ox[6] = { 0, -1, 1, 0, 0, 0 }, oy[6] = { 0, 0, 0, -1, 1, 0 };
    lg__ctx *c = &lg__c;
    lg__rect r, sq, land = lg__r(0, 0, 0, 0);
    lg__box *top = 0, *other = 0;
    float q = 34, cx, cy;
    int n, i, side, pass, under = -1, layer = c->layer, pick = 0, pick_n = 0;
    /* what is under the mouse, apart from b itself: a docked window, a floating one that may be docked onto, or the free middle */
    for (i = 0; i < LOGUIA_MAX_BOXES; i++) {
        lg__box *o = &c->boxes[i];
        if (o == b || o->back == 1 || !lg__box_live(o) || !lg__in(o->rect, c->mx, c->my)) continue;
        if (!top || lg__level(o) > lg__level(top)) top = o;
    }
    if (top && top->dock) under = top->dock - 1;
    else if (top && !top->back && (top->flags & LG_WINDOW_DOCK)) other = top;
    for (n = 1; n < LOGUIA_MAX_DOCKS && !top; n++) if (c->docks[n].kind == LG__D_HOLE && lg__in(c->docks[n].rect, c->mx, c->my)) under = n;
    c->layer = 0x7FFFFFE0;
    lg__push_clip_raw(lg__r(0, 0, c->w, c->h));
    for (pass = 0; pass < 2; pass++) {              /* the squares for the whole screen, then for what is under the mouse */
        int kind = pass && other ? LG__D_LEAF : c->docks[pass && under >= 0 ? under : 0].kind;
        if (pass && under < 0 && !other) break;
        n = pass ? under : 0;
        r = pass && other ? other->rect : c->docks[n].rect;
        for (side = LG_DOCK_LEFT; side <= LG_DOCK_TAB; side++) {
            int hot;
            if (side == LG_DOCK_TAB && kind >= LG__D_COLS) continue;
            cx = r.x + r.w / 2 + ox[side] * (pass ? q + 4 : r.w / 2 - q / 2 - 8);
            cy = r.y + r.h / 2 + oy[side] * (pass ? q + 4 : r.h / 2 - q / 2 - 8);
            sq = lg__r(lg__floor(cx - q / 2), lg__floor(cy - q / 2), q, q);
            hot = lg__in(sq, c->mx, c->my);
            if (hot) { pick = side; pick_n = pass && other ? -1 : n; land = lg__dock_part(r, side, kind == LG__D_LEAF ? 0.5f : 0.25f); }
            lg__fillr(sq, lg__col(hot ? LG_COL_BUTTON_ACTIVE : LG_COL_BUTTON));
            lg__outliner(sq, lg__max(c->style.border, 1), lg__col(LG_COL_BORDER));
            lg__fill(lg__dock_part(lg__r(sq.x + 7, sq.y + 7, q - 14, q - 14), side, 0.45f), lg__col(LG_COL_ACCENT));     /* a small picture of the result */
        }
    }
    if (pick) {
        lg__fill(land, (lg__col(LG_COL_ACCENT) & 0xFFFFFF00u) | 0x50);
        if (c->mreleased[0] && !c->drop_side) { c->drop_id = b->id; c->drop_node = pick_n; c->drop_other = other ? other->id : 0; c->drop_side = pick; }
    }
    lg__pop_clip();
    c->layer = layer;
}

/* ---------------------------------------------------------------- setup, input, frames */

LGDEF void lg_init(void) {
    memset(&lg__c, 0, sizeof lg__c);
    lg__c.pixel_scale = 1;
    lg__c.docks[0].kind = LG__D_HOLE;
    lg__c.docks[0].parent = -1;
    lg_style_dark();
#ifndef LOGUIA_NO_DEFAULT_FONT
    lg__default_font(0);
#endif
}
LGDEF void lg_shutdown(void) {
    while (lg__c.nundo > 0) LOGUIA_FREE(lg__c.undo[--lg__c.nundo].text);
    while (lg__c.nredo > 0) LOGUIA_FREE(lg__c.redo[--lg__c.nredo].text);
    LOGUIA_FREE(lg__c.own_pixels); LOGUIA_FREE(lg__c.own_pixels2); LOGUIA_FREE(lg__c.v); LOGUIA_FREE(lg__c.idx);
    LOGUIA_FREE(lg__c.cmds); LOGUIA_FREE(lg__c.cmd_layer);
    memset(&lg__c, 0, sizeof lg__c);
}

LGDEF void lg_input_mouse_pos(float x, float y) { lg__c.mx = x; lg__c.my = y; }
LGDEF void lg_input_mouse_button(int button, int down) {
    if (button < 0 || button > 2) return;
    if (down && !lg__c.mdown[button]) lg__c.mpressed[button] = 1;
    if (!down && lg__c.mdown[button]) lg__c.mreleased[button] = 1;
    lg__c.mdown[button] = down != 0;
}
LGDEF void lg_input_scroll(float dx, float dy) { (void)dx; lg__c.wheel += dy; }
LGDEF void lg_input_key(int key, int down) {
    if (key <= 0 || key >= LG_KEY_COUNT) return;
    lg__c.key_down[key] = down != 0;
    if (down && lg__c.key_pressed[key] < 255) lg__c.key_pressed[key]++;
    /* remembered per key press: a quick Ctrl+A can begin and end between two frames */
    if (down && lg__c.key_down[LG_KEY_CTRL]) lg__c.key_ctrl[key] = 1;
    if (down && lg__c.key_down[LG_KEY_SHIFT]) lg__c.key_shift[key] = 1;
}
LGDEF void lg_input_char(unsigned int cp) {
    if (cp >= 32 && cp != 127 && lg__c.nchars < 32) lg__c.chars[lg__c.nchars++] = cp;
}
LGDEF void lg_input_compose(const char *text, int caret) {
    lg__ctx *c = &lg__c;
    int n = text ? (int)strlen(text) : 0;
    if (n > (int)sizeof c->compose - 1) n = (int)sizeof c->compose - 1;
    while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80) n--;        /* whole characters only */
    if (n) memcpy(c->compose, text, (size_t)n);
    c->compose[n] = 0;
    c->compose_caret = caret < 0 || caret > n ? n : caret;
}
LGDEF int lg_ime_rect(float *x, float *y, float *w, float *h) {
    lg__ctx *c = &lg__c;
    int on = c->focus && c->ime_frame >= c->frame - 1 && c->ime_frame > 0;
    if (x) *x = on ? c->ime.x : 0;
    if (y) *y = on ? c->ime.y : 0;
    if (w) *w = on ? c->ime.w : 0;
    if (h) *h = on ? c->ime.h : 0;
    return on;
}
LGDEF void lg_set_clipboard(const char *(*get)(void *), void (*set)(void *, const char *), void *user) {
    lg__c.clip_get = get; lg__c.clip_set = set; lg__c.clip_user = user;
}
LGDEF int lg_wants_mouse(void) { return lg__c.hover_win != 0 || lg__c.active != 0; }
LGDEF int lg_wants_keyboard(void) { return lg__c.focus != 0 || lg__c.nav_show; }
LGDEF void lg_set_keyboard_nav(int on) { lg__c.nav_off = !on; if (!on) lg__c.nav_show = 0; }

static void lg__close_popups(int keep) {
    if (keep < lg__c.npopups) { lg__c.npopups = keep; lg__c.nav_id = lg__c.popup_nav[keep]; }
}

LGDEF void lg_begin_frame(float width, float height, float dt) {
    lg__ctx *c = &lg__c;
    int i;
    LOGUIA_ASSERT(c->font.pixels && "lg_init() or lg_set_font() first");
    c->w = width; c->h = height; c->dt = dt; c->time += dt;
    c->frame++;
    c->nv = c->ni = c->ncmd = c->nclips = c->nids = 0;
    if (c->font.source && ((lg__tt *)c->font.source)->full) lg__tt_reset((lg__tt *)c->font.source, c->pixel_scale);
    c->box = c->win = c->scroll_box = 0;
    c->layer = 0;
    c->next_w = 0;
    c->focus_seen = c->focus_keep = 0;
    c->in_bar = c->main_bar = 0;
    lg__dock_frame();
    /* the window under the mouse is the top one that was shown last frame */
    if (!c->active) {
        c->hover_win = 0;
        for (i = 0; i < LOGUIA_MAX_BOXES; i++) {
            lg__box *b = &c->boxes[i];
            if (b->id && b->is_window && b->last_frame == c->frame - 1 && lg__in(b->rect, c->mx, c->my) &&
                (!c->hover_win || lg__level(b) > lg__level(c->hover_win))) c->hover_win = b;
        }
        if (c->hover_win && c->mpressed[0] && !c->hover_win->is_popup && c->hover_win->z != c->top_z) {
            c->hover_win->z = ++c->top_z;
            if (c->hover_win->dock && lg__dock_root(c->hover_win->dock - 1)) c->docks[lg__dock_root(c->hover_win->dock - 1)].z = c->top_z;     /* its whole group comes up */
        }
    }
    /* popups: forget the ones nobody showed, close on Escape or on a click somewhere else */
    while (c->npopups && c->popup_seen[c->npopups - 1] < c->frame - 1) lg__close_popups(c->npopups - 1);
    if (c->npopups && c->key_pressed[LG_KEY_ESCAPE]) lg__close_popups(c->npopups - 1);
    else if (c->npopups && (c->mpressed[0] || c->mpressed[1] || c->mpressed[2])) {
        int keep = 0;
        for (i = 0; i < c->npopups; i++) if (c->hover_win && c->hover_win->id == c->popups[i]) keep = i + 1;
        if (keep < c->npopups) { c->popup_closed = c->popups[keep]; lg__close_popups(keep); }
    } else if (c->key_pressed[LG_KEY_ESCAPE] && !c->focus) c->nav_show = 0;
    c->pdepth = 0;
    c->table.on = 0;
    lg__dock_splits();
    /* keyboard navigation: which way to go this frame */
    c->nav_first = c->nav_last = 0;
    c->nav_req = c->nav_take = c->nav_wrap = 0;
    if (!c->focus) c->edit_multi = 0;
    if (!c->nav_off && !c->edit_multi) {
        if (c->key_pressed[LG_KEY_TAB]) c->nav_req = c->key_shift[LG_KEY_TAB] ? -1 : 1;
        else if (!c->focus && c->key_pressed[LG_KEY_DOWN]) c->nav_req = 1;
        else if (!c->focus && c->key_pressed[LG_KEY_UP]) c->nav_req = -1;
        if (c->nav_auto && c->nav_show) c->nav_req = 1;     /* a popup opened: step into it */
    }
    c->nav_auto = 0;
    if (c->nav_req) { c->nav_show = 1; c->nav_arrived = 0; }
}

LGDEF void lg_end_frame(void) {
    lg__ctx *c = &lg__c;
    int i, j;
    LOGUIA_ASSERT(!c->box && "lg_begin without lg_end");
    /* windows draw in their stacking order; inside one window the order stays as it was made */
    for (i = 1; i < c->ncmd; i++) {
        lg_cmd cmd = c->cmds[i];
        int layer = c->cmd_layer[i];
        for (j = i; j > 0 && c->cmd_layer[j - 1] > layer; j--) {
            c->cmds[j] = c->cmds[j - 1];
            c->cmd_layer[j] = c->cmd_layer[j - 1];
        }
        c->cmds[j] = cmd;
        c->cmd_layer[j] = layer;
    }
    c->list.vertices = c->v;  c->list.vertex_count = c->nv;
    c->list.indices = c->idx; c->list.index_count = c->ni;
    c->list.cmds = c->cmds;   c->list.cmd_count = c->ncmd;

    if (c->scroll_box && c->wheel != 0) c->scroll_box->scroll -= c->wheel * lg__line_h() * 3;
    /* a step that found no next widget starts again from the other end */
    if (c->nav_req > 0) c->nav_id = c->nav_first;
    else if (c->nav_req < 0 || c->nav_wrap) c->nav_id = c->nav_last;
    if (c->nav_req || c->nav_wrap) { c->nav_arrived = c->nav_id; c->nav_scroll = 1; }
    c->popup_closed = 0;
    if (!c->mdown[0]) c->active = 0;
    if ((c->mpressed[0] && !c->focus_keep) || !c->focus_seen) c->focus = 0;
    c->wheel = 0;
    c->pmx = c->mx; c->pmy = c->my;
    c->nchars = 0;
    memset(c->mpressed, 0, sizeof c->mpressed);
    memset(c->mreleased, 0, sizeof c->mreleased);
    memset(c->key_pressed, 0, sizeof c->key_pressed);
    memset(c->key_ctrl, 0, sizeof c->key_ctrl);
    memset(c->key_shift, 0, sizeof c->key_shift);
}
LGDEF const lg_draw_list *lg_get_draw_list(void) { return &lg__c.list; }

/* ---------------------------------------------------------------- interaction */

static int lg__hover(lg__rect r) {
    return lg__c.win && lg__c.win == lg__c.hover_win && lg__in(lg__clip(), lg__c.mx, lg__c.my) && lg__in(r, lg__c.mx, lg__c.my);
}
/* the heart of every clickable thing: returns 1 when the mouse was pressed and let go on it */
static int lg__behavior(unsigned int id, lg__rect r, int *hovered, int *held) {
    lg__ctx *c = &lg__c;
    int hov = lg__hover(r) && (!c->active || c->active == id), clicked = 0;
    c->item_id = id;
    c->item_pressed = c->item_key = 0;
    if (hov && c->mpressed[0] && !c->active) { c->active = id; c->item_pressed = 1; c->press_x = c->mx; c->press_y = c->my; }
    if (c->active == id && c->mreleased[0]) clicked = hov;
    c->item_hovered = hov;
    if (hovered) *hovered = hov;
    if (held) *held = c->active == id && (c->mdown[0] || c->item_pressed);
    return clicked;
}
/* lg__behavior for real widgets: they also take part in keyboard navigation, and Enter or
   Space on the marked one counts as a click (item_key says it was the keyboard) */
static int lg__widget(unsigned int id, lg__rect r, int *hovered, int *held) {
    lg__ctx *c = &lg__c;
    int clicked = lg__behavior(id, r, hovered, held);
    if (c->nav_off || (c->win && c->win->hidden)) return clicked;
    /* while a popup is open the keyboard stays inside the innermost one */
    if (c->npopups && !(c->win && c->win->is_popup && c->win->id == c->popups[c->npopups - 1])) return clicked;
    if (c->item_pressed) { c->nav_id = id; c->nav_show = 0; }
    if (!c->nav_first) c->nav_first = id;
    if (c->nav_take) { c->nav_id = c->nav_arrived = id; c->nav_take = c->nav_req = 0; c->nav_scroll = 1; }
    else if (c->nav_req > 0 && c->nav_id == id) c->nav_take = 1;
    else if (c->nav_req < 0 && c->nav_id == id) {
        if (c->nav_last) { c->nav_id = c->nav_arrived = c->nav_last; c->nav_scroll = 1; } else c->nav_wrap = 1;
        c->nav_req = 0;
    }
    c->nav_last = id;
    if (c->nav_id == id && c->nav_show) {
        if (c->nav_scroll && c->box) {          /* bring it into view */
            lg__box *b = c->box;
            float top = b->inner.y + b->pad, bottom = b->inner.y + b->inner.h - b->pad;
            if (r.y < top) b->scroll -= top - r.y;
            else if (r.y + r.h > bottom) b->scroll += r.y + r.h - bottom;
            c->nav_scroll = 0;
        }
        if (!c->focus && (c->key_pressed[LG_KEY_ENTER] || c->key_pressed[LG_KEY_SPACE])) c->item_key = clicked = 1;
    }
    return clicked;
}
/* the mark around the widget the keyboard is on */
static void lg__ring(lg__rect r) {
    if (lg__c.nav_show && lg__c.item_id && lg__c.nav_id == lg__c.item_id)
        lg__outliner(r, lg__max(lg__c.style.border, 1) + 1, lg__col(LG_COL_ACCENT));
}
LGDEF int lg_item_hovered(void) { return lg__c.item_hovered; }
LGDEF int lg_item_clicked(void) { return lg__c.item_pressed; }
LGDEF int lg_item_active(void) { return lg__c.item_id && lg__c.active == lg__c.item_id; }
LGDEF void lg_item_rect(float *x, float *y, float *w, float *h) {
    if (x) *x = lg__c.item_rect.x;
    if (y) *y = lg__c.item_rect.y;
    if (w) *w = lg__c.item_rect.w;
    if (h) *h = lg__c.item_rect.h;
}

/* ---------------------------------------------------------------- layout */

static float lg__next_x(void) {
    lg__layout *l = &lg__c.box->lay;
    return l->same_line ? l->line_x + lg__c.style.spacing_x : l->x0 + l->indent;
}
/* 0 = all that is left, above 0 = that many pixels, below 0 = all that is left minus that */
static float lg__size(float want, float avail) {
    return lg__max(0, lg__floor(want > 0 ? want : avail + want));
}
LGDEF float lg_available_width(void) {
    LOGUIA_ASSERT(lg__c.box && "widgets go between lg_begin and lg_end");
    return lg__max(0, lg__c.box->lay.x1 - lg__next_x());
}
/* the width a widget gets: what lg_set_next_width asked for, or its own default */
static float lg__width(float normal) {
    float w = lg__c.next_w != 0 ? lg__size(lg__c.next_w, lg_available_width()) : normal > 0 ? normal : lg_available_width();
    lg__c.next_w = 0;
    return w;
}
static lg__rect lg__place(float w, float h) {
    lg__layout *l;
    float x, y;
    LOGUIA_ASSERT(lg__c.box && "widgets go between lg_begin and lg_end");
    l = &lg__c.box->lay;
    w = lg__floor(w); h = lg__floor(h);
    x = lg__next_x();
    if (l->same_line) {
        y = l->line_y;
        if (h < l->line_h) y += lg__floor((l->line_h - h) / 2);     /* centre short things on the line */
        else l->line_h = h;
    } else {
        y = l->line_y = l->cy;
        l->line_h = h;
    }
    l->line_x = x + w;
    l->cy = l->line_y + l->line_h + lg__c.style.spacing_y;
    l->max_x = lg__max(l->max_x, x + w);
    l->max_y = lg__max(l->max_y, l->line_y + l->line_h);
    l->same_line = 0;
    l->has_item = 1;
    lg__c.item_id = 0; lg__c.item_pressed = lg__c.item_key = 0;
    lg__c.item_rect = lg__r(x, y, w, h);
    lg__c.item_hovered = lg__hover(lg__c.item_rect);
    return lg__c.item_rect;
}
LGDEF void lg_same_line(void) { if (lg__c.box && lg__c.box->lay.has_item) lg__c.box->lay.same_line = 1; }
LGDEF void lg_set_next_width(float w) { lg__c.next_w = w; }
LGDEF void lg_spacing(float height) { if (lg__c.box) lg__c.box->lay.cy += height; }
LGDEF void lg_indent(float w) { if (lg__c.box) lg__c.box->lay.indent += w > 0 ? w : lg__c.style.indent; }
LGDEF void lg_unindent(float w) { if (lg__c.box) lg__c.box->lay.indent -= w > 0 ? w : lg__c.style.indent; }
LGDEF void lg_separator(void) {
    lg__layout *l = &lg__c.box->lay;
    lg__rect r;
    l->same_line = 0;
    r = lg__place(l->x1 - l->x0 - l->indent, 1);
    lg__fill(r, lg__col(LG_COL_SEPARATOR));
}
LGDEF void lg_reserve(float w, float h, float *x, float *y) {
    lg__rect r = lg__place(lg__width(w), h);
    if (x) *x = r.x;
    if (y) *y = r.y;
}
LGDEF int lg_interact(const char *name, float x, float y, float w, float h) {
    return lg__widget(lg__id(name), lg__r(x, y, w, h), 0, 0);
}
LGDEF void lg_push_id(int n) { lg__push(lg__hash(&n, (int)sizeof n, lg__seed())); }
LGDEF void lg_push_id_str(const char *s) { lg__push(lg__id(s)); }
LGDEF void lg_pop_id(void) { if (lg__c.nids) lg__c.nids--; }

/* ---------------------------------------------------------------- windows and panels */

static lg__box *lg__get_box(unsigned int id, int *is_new) {
    lg__box *spare = 0, *oldest = 0, *b;
    int i;
    *is_new = 0;
    for (i = 0; i < LOGUIA_MAX_BOXES; i++) {
        b = &lg__c.boxes[i];
        if (b->id == id) return b;
        if (!b->id) { if (!spare) spare = b; }
        else if (!oldest || b->last_frame < oldest->last_frame) oldest = b;
    }
    b = spare ? spare : oldest;     /* all in use: take over the one unseen for the longest */
    LOGUIA_ASSERT(b->last_frame != lg__c.frame && "too many windows and panels: raise LOGUIA_MAX_BOXES");
    memset(b, 0, sizeof *b);
    b->id = id;
    *is_new = 1;
    return b;
}

/* b->pad (the space around the contents) has to be set before this */
static void lg__box_begin(lg__box *b, lg__rect inner, int flags) {
    lg__ctx *c = &lg__c;
    float pad = b->pad, view = inner.h - 2 * pad;
    b->flags = flags;
    b->inner = inner;
    b->last_frame = c->frame;
    b->has_bar = !(flags & LG_WINDOW_NO_SCROLL) && b->content_h > view + 0.5f;
    b->scroll = b->has_bar ? lg__clamp(lg__floor(b->scroll), 0, b->content_h - view) : 0;
    c->box = b;
    lg__push(b->id);
    lg__push_clip(inner);
    memset(&b->lay, 0, sizeof b->lay);
    b->lay.x0 = b->lay.max_x = inner.x + pad;
    b->lay.x1 = inner.x + inner.w - pad - (b->has_bar ? c->style.scrollbar_size : 0);
    b->lay.cy = b->lay.max_y = inner.y + pad - b->scroll;
    if (b->has_bar && c->win == c->hover_win && lg__in(lg__clip(), c->mx, c->my)) c->scroll_box = b;
}

static void lg__box_end(lg__box *b) {
    lg__ctx *c = &lg__c;
    float pad = b->pad, view = b->inner.h - 2 * pad;
    b->content_w = b->lay.max_x - b->lay.x0;
    b->content_h = b->lay.max_y - (b->inner.y + pad - b->scroll);
    if (b->has_bar) {
        float size = c->style.scrollbar_size, range = b->content_h - view;
        lg__rect track = lg__r(b->inner.x + b->inner.w - size, b->inner.y, size, b->inner.h), thumb;
        float th = lg__max(20, lg__floor(track.h * view / b->content_h)), travel = track.h - th;
        int hov, held;
        lg__behavior(lg__id("#scroll"), track, &hov, &held);
        if (held && travel > 0) {
            if (c->item_pressed) {       /* a click on the track jumps there */
                thumb = lg__r(track.x, track.y + travel * b->scroll / range, size, th);
                if (!lg__in(thumb, c->mx, c->my)) b->scroll = (c->my - track.y - th / 2) / travel * range;
                b->scroll = lg__clamp(b->scroll, 0, range);
                c->grab_y = c->my - (track.y + travel * b->scroll / range);
            } else b->scroll = (c->my - c->grab_y - track.y) / travel * range;
            b->scroll = lg__clamp(b->scroll, 0, range);
        }
        thumb = lg__r(track.x + 2, track.y + (range > 0 ? lg__floor(travel * b->scroll / range) : 0), size - 4, th);
        lg__fill(track, lg__col(LG_COL_SCROLLBAR));
        lg__fillr(thumb, lg__col(hov || held ? LG_COL_SCROLL_THUMB_HOVER : LG_COL_SCROLL_THUMB));
    }
    lg__pop_clip();
    lg_pop_id();
}

#define LG__WINDOW_BAR 0x10000      /* the main menu bar */

LGDEF int lg_begin(const char *title, float x, float y, float w, float h, int flags) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    const char *title_end = lg__label_end(title);
    int is_new, hov = 0, held = 0, i, tabs = 0, front = 1;
    float th, bd = s->border, rad = s->rounding, top = c->bar_frame >= c->frame - 1 ? c->bar_h : 0, before = 0, total = 0;
    unsigned int tid;
    lg__dock *d = 0;
    lg__rect tr, wr;
    lg__box *b;
    LOGUIA_ASSERT(!c->box && "windows cannot be inside windows: use lg_begin_panel");
    c->nids = c->nclips = 0;
    b = lg__get_box(lg__id(title), &is_new);
    if (is_new) b->z = ++c->top_z;
    if (!b->placed) { b->placed = 1; b->rect = lg__r(x, y, w, h); b->fw = w; b->fh = h; }
    b->is_window = 1;
    b->is_popup = 0;
    b->parent = 0;
    b->pad = s->window_padding;
    b->tab_w = lg__text_w(title, title_end) + 2 * s->padding_x;
    if (b->dock && c->docks[b->dock - 1].kind != LG__D_LEAF) b->dock = 0;
    if (flags & LG__WINDOW_BAR) { b->rect = lg__r(0, 0, c->w, h); b->bar = c->frame; bd = rad = 0; }
    else if (b->dock) {         /* its place comes from the dock; its title is a tab when it shares that place */
        d = &c->docks[b->dock - 1];
        total = b->tab_w;
        for (i = 0; i < LOGUIA_MAX_BOXES; i++) {
            lg__box *o = &c->boxes[i];
            if (o == b || o->dock != b->dock || !lg__box_live(o)) continue;
            tabs++; total += o->tab_w;
            if (o->order < b->order) before += o->tab_w;
        }
        if (!d->tab) d->tab = b->id;
        front = d->tab == b->id;
        flags = (flags | LG_WINDOW_NO_RESIZE) & ~LG_WINDOW_FILL;
        if (tabs) flags &= ~LG_WINDOW_NO_TITLE;
        rad = 0;
    } else if (flags & LG_WINDOW_FILL) { b->rect = lg__r(0, top, c->w, c->h - top); flags |= LG_WINDOW_NO_MOVE | LG_WINDOW_NO_RESIZE; bd = rad = 0; }
    b->back = d || (flags & LG__WINDOW_BAR) ? 2 : (flags & LG_WINDOW_FILL) ? 1 : 0;
    b->hidden = !front;
    th = (flags & LG_WINDOW_NO_TITLE) ? 0 : lg__line_h() + 2 * s->padding_y;
    c->win = b;
    c->layer = lg__level(b);
    lg__push(b->id);
    tid = lg__id("#title");

    if (d) {
        float k = total > d->rect.w ? d->rect.w / total : 1;       /* too many tabs: they all get narrower */
        tr = tabs ? lg__r(d->rect.x + lg__floor(before * k), d->rect.y, lg__floor(b->tab_w * k), th) : lg__r(d->rect.x, d->rect.y, d->rect.w, th);
        b->rect = front ? d->rect : tr;
        if (th > 0) {
            lg__behavior(tid, tr, &hov, &held);
            if (c->item_pressed) { c->grab_x = c->mx - tr.x; c->grab_y = c->my - tr.y; d->want = b->id; }
            else if (held && (flags & LG_WINDOW_DOCK) && !c->drop_side && lg__far(8)) { c->drop_id = b->id; c->drop_side = -1; }
        }
    } else {
        if (!(flags & LG_WINDOW_NO_RESIZE)) {
            lg__behavior(lg__id("#resize"), lg__r(b->rect.x + b->rect.w - 14, b->rect.y + b->rect.h - 14, 14, 14), &hov, &held);
            if (c->item_pressed) { c->grab_x = b->rect.x + b->rect.w - c->mx; c->grab_y = b->rect.y + b->rect.h - c->my; }
            else if (held) { b->rect.w = lg__floor(c->mx + c->grab_x - b->rect.x); b->rect.h = lg__floor(c->my + c->grab_y - b->rect.y); }
            b->rect.w = lg__max(b->rect.w, 120);
            b->rect.h = lg__max(b->rect.h, th + 60);
        }
        if (th > 0) {
            lg__behavior(tid, lg__r(b->rect.x, b->rect.y, b->rect.w, th), &hov, &held);
            if (c->item_pressed) { c->grab_x = c->mx - b->rect.x; c->grab_y = c->my - b->rect.y; }
            else if (held && !(flags & LG_WINDOW_NO_MOVE)) { b->rect.x = c->mx - c->grab_x; b->rect.y = c->my - c->grab_y; }
        }
        if (!(flags & (LG_WINDOW_FILL | LG__WINDOW_BAR))) {      /* never let a window get lost outside the screen */
            b->rect.x = lg__floor(lg__clamp(b->rect.x, 40 - b->rect.w, lg__max(0, c->w - 40)));
            b->rect.y = lg__floor(lg__clamp(b->rect.y, top, lg__max(top, c->h - lg__max(th, 20))));
        }
        tr = lg__r(b->rect.x, b->rect.y, b->rect.w, th);
        if ((flags & LG_WINDOW_DOCK) && th > 0 && !(flags & LG_WINDOW_NO_MOVE) && c->active == tid && lg__far(8)) lg__dock_targets(b);
    }
    lg_pop_id();
    wr = d ? d->rect : b->rect;

    if (front) {
        if (!(flags & LG_WINDOW_NO_BACKGROUND)) lg__fill_round(wr, lg__col(LG_COL_WINDOW), rad, 15);
        if (th > 0 && tabs) {
            lg__fill(lg__r(wr.x, wr.y, wr.w, th), lg__col(LG_COL_TITLE));
            lg__fill(tr, lg__col(b->z == c->top_z ? LG_COL_TITLE_ACTIVE : LG_COL_BUTTON));
            lg__fill(lg__r(tr.x, tr.y + th - 2, tr.w, 2), lg__col(LG_COL_ACCENT));
        } else if (th > 0) lg__fill_round(tr, lg__col(b->z == c->top_z ? LG_COL_TITLE_ACTIVE : LG_COL_TITLE), rad, 3);
    } else if (hov) lg__fill(tr, lg__col(LG_COL_HOVER));
    if (th > 0) {
        lg__push_clip(tr);
        lg__text(tr.x + s->padding_x + (tabs ? 0 : bd), tr.y + s->padding_y, title, title_end, lg__col(front ? LG_COL_TEXT : LG_COL_TEXT_DIM));
        lg__pop_clip();
    }
    if (!front) lg__push_clip_raw(lg__r(0, 0, 0, 0));       /* behind another tab: its contents are laid out, but nothing shows */
    b->bar_y = wr.y + th;
    b->bar_h = b->bar > 0 && b->bar >= c->frame - 1 ? lg__line_h() + 2 * s->padding_y : 0;
    lg__box_begin(b, lg__r(wr.x + bd, wr.y + th + b->bar_h, wr.w - 2 * bd, lg__max(0, wr.h - th - b->bar_h - bd)), flags);
    return front;
}

LGDEF void lg_end(void) {
    lg__ctx *c = &lg__c;
    lg__box *b = c->box;
    float rad;
    LOGUIA_ASSERT(b && b->is_window && !b->is_popup && "lg_end without lg_begin (or a panel or popup was left open)");
    LOGUIA_ASSERT(!c->table.on && "lg_begin_table without lg_end_table");
    LOGUIA_ASSERT(!c->in_bar && "lg_begin_menu_bar without lg_end_menu_bar");
    rad = b->back ? 0 : c->style.rounding;
    lg__box_end(b);
    if (!b->hidden && !(b->flags & LG_WINDOW_NO_RESIZE)) {
        float in = 2 + rad * 0.3f, x = b->rect.x + b->rect.w - in, y = b->rect.y + b->rect.h - in;
        lg__tri(x, y - 10, x, y, x - 10, y, lg__col(LG_COL_BORDER));
    }
    if (!b->hidden && !(b->flags & LG_WINDOW_NO_BACKGROUND)) lg__outline_round(b->rect, c->style.border, lg__col(LG_COL_BORDER), rad, 15);
    c->box = c->win = 0;
    c->layer = 0;
    c->nids = c->nclips = 0;
}

LGDEF void lg_dock_window(const char *title, const char *next_to, int side, float share) {
    lg__ctx *c = &lg__c;
    unsigned int id = lg__hash(title, (int)strlen(title), 2166136261u);
    unsigned int other = next_to ? lg__hash(next_to, (int)strlen(next_to), 2166136261u) : 0;
    int is_new, i, n = 0;
    lg__box *b = lg__get_box(id, &is_new);
    if (is_new) { b->is_window = 1; b->z = ++c->top_z; b->last_frame = c->frame; }      /* lg_begin has not seen it yet */
    if (b->dock) { lg__undock(b); lg__dock_layout(); }
    if (side >= LG_DOCK_LEFT && side <= LG_DOCK_TAB) {
        for (i = 0; other && i < LOGUIA_MAX_BOXES; i++) {
            lg__box *o = &c->boxes[i];
            if (o->id != other || !o->is_window || o == b) continue;
            if (o->dock) n = o->dock - 1;
            else if (o->placed && (n = lg__dock_group(o)) == 0) return;       /* next to a floating window: the two become a group */
        }
        lg__dock_put(b, n, side, share);
    }
    lg__dock_layout();
}
LGDEF void lg_dock_space(float *x, float *y, float *w, float *h) {
    lg__rect r = lg__r(0, 0, 0, 0);
    int n;
    for (n = 0; n < LOGUIA_MAX_DOCKS; n++) if (lg__c.docks[n].kind == LG__D_HOLE) r = lg__c.docks[n].rect;
    if (x) *x = r.x;
    if (y) *y = r.y;
    if (w) *w = r.w;
    if (h) *h = r.h;
}

/* fill, line: colour numbers, or -1 for none */
static lg__box *lg__panel(unsigned int id, float w, float h, int flags, float pad, int fill, int line) {
    lg__ctx *c = &lg__c;
    lg__box *parent = c->box, *b;
    lg__rect r;
    int is_new;
    float left_h, bd = line >= 0 ? c->style.border : 0;
    LOGUIA_ASSERT(parent && "panels go inside a window");
    left_h = parent->inner.y + parent->inner.h - parent->pad - (parent->lay.same_line ? parent->lay.line_y : parent->lay.cy);
    w = lg__size(w, lg_available_width());
    h = h > 0 ? lg__floor(h) : lg__max(lg__size(h, left_h), 4 * lg__line_h());     /* never squeezed to nothing */
    r = lg__place(w, h);
    b = lg__get_box(id, &is_new);
    LOGUIA_ASSERT((is_new || b->last_frame != c->frame) && "two panels with the same name");
    b->is_window = b->is_popup = 0;
    b->parent = parent;
    b->rect = r;
    b->pad = pad;
    if (fill >= 0) lg__fillr(r, lg__col(fill));
    if (line >= 0) lg__outliner(r, bd, lg__col(line));
    lg__box_begin(b, lg__r(r.x + bd, r.y + bd, r.w - 2 * bd, r.h - 2 * bd), flags);
    return b;
}
LGDEF void lg_begin_panel(const char *name, float w, float h, int flags) {
    int bg = !(flags & LG_WINDOW_NO_BACKGROUND);
    lg__panel(lg__id(name), w, h, flags, lg__c.style.window_padding, bg ? LG_COL_PANEL : -1, bg ? LG_COL_BORDER : -1);
}
LGDEF void lg_end_panel(void) {
    lg__box *b = lg__c.box;
    LOGUIA_ASSERT(b && !b->is_window && "lg_end_panel without lg_begin_panel");
    lg__box_end(b);
    lg__c.box = b->parent;
}

/* ---------------------------------------------------------------- popups */

static void lg__open_popup(unsigned int id, float x, float y, float min_w) {
    lg__ctx *c = &lg__c;
    int d = c->pdepth;
    if (d >= LOGUIA_MAX_POPUPS) return;
    lg__close_popups(d);
    c->popups[d] = id; c->popup_x[d] = x; c->popup_y[d] = y; c->popup_min_w[d] = min_w;
    c->popup_seen[d] = c->frame;
    c->popup_nav[d] = c->nav_id;        /* the keyboard comes back here when the popup closes */
    c->npopups = d + 1;
    c->nav_id = 0; c->nav_auto = 1;
}
static int lg__begin_popup(unsigned int id) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    int d = c->pdepth, is_new, fresh;
    float pad = s->padding_y + 2, bd = s->border, w, h, max_h = lg__floor(c->h * 0.6f);
    lg__saved *sv;
    lg__box *b;
    if (d >= c->npopups || c->popups[d] != id) return 0;
    c->popup_seen[d] = c->frame;
    b = lg__get_box(id, &is_new);
    fresh = is_new || b->last_frame < c->frame - 1;     /* its size is known only after one frame: stay hidden for that one */
    if (fresh) b->scroll = 0;
    w = b->content_w + 2 * pad + 2 * bd;
    h = b->content_h + 2 * pad + 2 * bd;
    if (h > max_h) { h = max_h; w += s->scrollbar_size; }
    w = lg__max(w, c->popup_min_w[d]);
    b->rect = lg__r(lg__floor(lg__clamp(c->popup_x[d], 0, lg__max(0, c->w - w))),
                    lg__floor(c->popup_y[d] + h > c->h ? lg__max(0, c->h - h) : c->popup_y[d]), lg__floor(w), lg__floor(h));
    b->is_window = b->is_popup = 1;
    b->parent = 0;
    b->pad = pad;
    b->z = 0x40000000 + d;
    sv = &c->saved[d];
    sv->box = c->box; sv->win = c->win; sv->layer = c->layer; sv->nids = c->nids; sv->nclips = c->nclips;
    sv->item_id = c->item_id; sv->item_rect = c->item_rect;     /* lg_item_* go on describing the widget before the popup */
    sv->item_hovered = c->item_hovered; sv->item_pressed = c->item_pressed; sv->item_key = c->item_key;
    c->win = b;
    c->layer = b->z;
    lg__push_clip_raw(fresh ? lg__r(0, 0, 0, 0) : lg__r(0, 0, c->w, c->h));
    lg__fillr(b->rect, lg__col(LG_COL_WINDOW));
    lg__box_begin(b, lg__r(b->rect.x + bd, b->rect.y + bd, b->rect.w - 2 * bd, b->rect.h - 2 * bd), 0);
    c->pdepth++;
    return 1;
}
LGDEF void lg_open_popup(const char *name) { lg__open_popup(lg__id(name), lg__c.mx, lg__c.my, 0); }
LGDEF int lg_begin_popup(const char *name) { return lg__begin_popup(lg__id(name)); }
LGDEF void lg_end_popup(void) {
    lg__ctx *c = &lg__c;
    lg__box *b = c->box;
    lg__saved *sv;
    LOGUIA_ASSERT(b && b->is_popup && c->pdepth > 0 && "lg_end_popup without a popup that returned 1");
    lg__box_end(b);
    lg__outliner(b->rect, c->style.border, lg__col(LG_COL_BORDER));
    sv = &c->saved[--c->pdepth];
    c->box = sv->box; c->win = sv->win; c->layer = sv->layer; c->nids = sv->nids; c->nclips = sv->nclips;
    c->item_id = sv->item_id; c->item_rect = sv->item_rect;
    c->item_hovered = sv->item_hovered; c->item_pressed = sv->item_pressed; c->item_key = sv->item_key;
}
LGDEF void lg_close_popup(void) { if (lg__c.pdepth > 0) lg__close_popups(lg__c.pdepth - 1); }

LGDEF void lg_tooltip(const char *text) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    const char *p, *e;
    float w = 0, h = 0, lh = lg__line_h(), x, y;
    int layer = c->layer;
    if (!c->item_hovered || c->active) return;
    for (p = text;; p = e + 1) {
        e = strchr(p, '\n');
        if (!e) e = p + strlen(p);
        w = lg__max(w, lg__text_w(p, e));
        h += lh;
        if (!*e) break;
    }
    w += 2 * s->padding_x; h += 2 * s->padding_y;
    x = lg__floor(lg__clamp(c->mx + 14, 0, lg__max(0, c->w - w)));
    y = lg__floor(c->my + 20 + h > c->h ? c->my - h - 6 : c->my + 20);
    c->layer = 0x7FFFFFF0;
    lg__push_clip_raw(lg__r(0, 0, c->w, c->h));
    lg__fillr(lg__r(x, y, w, h), lg__col(LG_COL_WINDOW));
    lg__outliner(lg__r(x, y, w, h), s->border, lg__col(LG_COL_BORDER));
    for (p = text, y += s->padding_y;; p = e + 1, y += lh) {
        e = strchr(p, '\n');
        if (!e) e = p + strlen(p);
        lg__text(x + s->padding_x, y, p, e, lg__col(LG_COL_TEXT));
        if (!*e) break;
    }
    lg__pop_clip();
    c->layer = layer;
}

/* ---------------------------------------------------------------- tables */

static void lg__table_row_end(void) {
    lg__ctx *c = &lg__c;
    lg__table *t = &c->table;
    t->bottom = lg__max(t->row_y + lg__line_h() + 2 * c->style.padding_y, c->box->lay.max_y + c->style.padding_y);
    if (t->bg >= 0) c->v[t->bg + 2].y = c->v[t->bg + 3].y = t->bottom;      /* the row's background: now its height is known */
    t->bg = -1;
    if (t->row == 0 && (t->flags & LG_TABLE_HEADER) && (t->flags & LG_TABLE_SORT)) {       /* headings that sort */
        unsigned int key = lg__hash("#sort", 5, t->id);
        int *sort = lg__state(key, 0), i, hov;
        float size = lg__line_h();
        for (i = 0; i < t->cols; i++) {
            lg__rect cell = lg__r(t->colx[i] + 4, t->row_y, t->colx[i + 1] - t->colx[i] - 8, t->bottom - t->row_y);
            if (lg__behavior(lg__hash(&i, (int)sizeof i, key), cell, &hov, 0)) *sort = (*sort >> 1) == i + 1 ? *sort ^ 1 : (i + 1) << 1;
            if (hov) lg__fill(cell, lg__col(LG_COL_HOVER));
            if ((*sort >> 1) == i + 1)
                lg__icon(*sort & 1 ? 0x25BC : 0x25B2, t->colx[i + 1] - size - 4, t->row_y + (t->bottom - t->row_y - size) / 2, size, lg__col(LG_COL_TEXT_DIM));
        }
    }
}
/* how far the user has dragged the line in front of column i */
static int *lg__table_shift(int i) { return lg__state(lg__hash(&i, (int)sizeof i, lg__hash("#col", 4, lg__c.table.id)), 0); }
LGDEF void lg_begin_table(const char *name, int columns, const float *widths, int flags) {
    lg__ctx *c = &lg__c;
    lg__table *t = &c->table;
    lg__layout *l;
    float total, fixed = 0, share;
    int i, shared = 0;
    LOGUIA_ASSERT(c->box && !t->on && "tables go inside a window, and not inside each other");
    LOGUIA_ASSERT(columns >= 1 && columns <= LOGUIA_MAX_COLUMNS && "raise LOGUIA_MAX_COLUMNS");
    l = &c->box->lay;
    l->same_line = 0;
    t->colx[0] = l->x0 + l->indent;
    total = lg__max(0, l->x1 - t->colx[0]);
    for (i = 0; i < columns; i++) { if (widths && widths[i] > 0) fixed += widths[i]; else shared++; }
    share = shared ? lg__max(0, total - fixed) / (float)shared : 0;
    for (i = 0; i < columns; i++) t->colx[i + 1] = t->colx[i] + (widths && widths[i] > 0 ? widths[i] : share);
    for (i = 0; i <= columns; i++) t->colx[i] = lg__floor(t->colx[i]);
    t->id = lg__id(name);
    for (i = 1; i < columns && (flags & LG_TABLE_RESIZE); i++)
        t->colx[i] = lg__max(t->colx[i - 1] + 24, lg__min(t->colx[i] + (float)*lg__table_shift(i), t->colx[columns] - 24 * (float)(columns - i)));
    t->on = 1; t->cols = columns; t->col = t->row = t->bg = -1; t->flags = flags;
    t->x0 = l->x0; t->x1 = l->x1; t->indent = l->indent;
    t->top = t->row_y = t->bottom = l->cy;
    lg_push_id_str(name);
}
LGDEF void lg_table_next(void) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    lg__table *t = &c->table;
    lg__layout *l = &c->box->lay;
    lg__rect cl;
    LOGUIA_ASSERT(t->on && "lg_table_next outside a table");
    if (t->col >= 0) { lg__pop_clip(); lg_pop_id(); }
    t->col++;
    if (t->col == t->cols || t->row < 0) {          /* a new row */
        int bg = (t->flags & LG_TABLE_HEADER) && t->row < 0 ? LG_COL_TITLE : (t->flags & LG_TABLE_STRIPES) && !(t->row & 1) ? LG_COL_HOVER : -1;
        if (t->row >= 0) lg__table_row_end();
        t->col = 0; t->row++; t->row_y = t->bottom;
        if (bg >= 0) {
            unsigned int *ix, base, col = lg__col(bg);
            lg_vertex *v = lg__prim(4, 6, 0, &ix, &base);
            lg__vert(v, t->colx[0], t->row_y, lg__wu(), lg__wv(), col);     lg__vert(v + 1, t->colx[t->cols], t->row_y, lg__wu(), lg__wv(), col);
            lg__vert(v + 2, t->colx[t->cols], t->row_y, lg__wu(), lg__wv(), col); lg__vert(v + 3, t->colx[0], t->row_y, lg__wu(), lg__wv(), col);
            ix[0] = base; ix[1] = base + 1; ix[2] = base + 2; ix[3] = base; ix[4] = base + 2; ix[5] = base + 3;
            t->bg = (int)base;
        }
        if ((t->flags & LG_TABLE_BORDERS) && t->row > 0)
            lg__fill(lg__r(t->colx[0], t->row_y, t->colx[t->cols] - t->colx[0], s->border), lg__col(LG_COL_BORDER));
    }
    l->x0 = t->colx[t->col] + s->padding_x; l->x1 = t->colx[t->col + 1] - s->padding_x; l->indent = 0;
    l->cy = t->row_y + s->padding_y;
    l->same_line = l->has_item = 0;
    cl = lg__clip();
    lg__push_clip(lg__r(t->colx[t->col], cl.y, t->colx[t->col + 1] - t->colx[t->col], cl.h));
    lg_push_id(t->row * t->cols + t->col);          /* the same label may be used in every cell */
}
LGDEF void lg_end_table(void) {
    lg__ctx *c = &lg__c;
    lg__table *t = &c->table;
    lg__layout *l = &c->box->lay;
    int i;
    LOGUIA_ASSERT(t->on && "lg_end_table without lg_begin_table");
    if (t->col >= 0) { lg__pop_clip(); lg_pop_id(); }
    if (t->row >= 0) lg__table_row_end();
    for (i = 1; i < t->cols && (t->flags & LG_TABLE_RESIZE); i++) {         /* the lines between columns can be dragged */
        int hov, held, *shift = lg__table_shift(i);
        lg__behavior(lg__hash(&i, (int)sizeof i, lg__hash("#line", 5, t->id)), lg__r(t->colx[i] - 3, t->top, 7, t->bottom - t->top), &hov, &held);
        if (c->item_pressed) c->grab_x = c->mx - (float)*shift;
        else if (held) *shift = (int)(c->mx - c->grab_x);
        if (hov || held) lg__fill(lg__r(t->colx[i] - 1, t->top, 3, t->bottom - t->top), lg__col(LG_COL_ACCENT));
    }
    lg_pop_id();
    if (t->flags & LG_TABLE_BORDERS) {
        lg__outline(lg__r(t->colx[0], t->top, t->colx[t->cols] - t->colx[0], t->bottom - t->top), c->style.border, lg__col(LG_COL_BORDER));
        for (i = 1; i < t->cols; i++) lg__fill(lg__r(t->colx[i], t->top, c->style.border, t->bottom - t->top), lg__col(LG_COL_BORDER));
    }
    l->x0 = t->x0; l->x1 = t->x1; l->indent = t->indent;
    l->line_y = t->top; l->line_h = t->bottom - t->top; l->line_x = t->colx[t->cols];
    l->cy = t->bottom + c->style.spacing_y;
    l->max_x = lg__max(l->max_x, t->colx[t->cols]);
    l->max_y = lg__max(l->max_y, t->bottom);
    l->same_line = 0; l->has_item = 1;
    t->on = 0;
}
LGDEF int lg_table_sort(int *column, int *descending) {
    int v;
    LOGUIA_ASSERT(lg__c.table.on && "lg_table_sort outside a table");
    v = *lg__state(lg__hash("#sort", 5, lg__c.table.id), 0);
    if (column) *column = (v >> 1) - 1;
    if (descending) *descending = v & 1;
    return v != 0;
}

/* ---------------------------------------------------------------- text */

LGDEF void lg_text_colored(unsigned int rgba, const char *text) {
    const char *s = text;
    for (;;) {                                  /* one layout line per line of text */
        const char *e = strchr(s, '\n');
        lg__rect r;
        if (!e) e = s + strlen(s);
        r = lg__place(lg__text_w(s, e), lg__line_h());
        lg__text(r.x, r.y, s, e, rgba);
        if (!*e) break;
        s = e + 1;
    }
}
LGDEF void lg_text(const char *text) { lg_text_colored(lg__col(LG_COL_TEXT), text); }

#ifndef LOGUIA_NO_STDIO
LGDEF void lg_textf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    lg_text(buf);
}
#endif

/* where a line that may be at most max_w wide has to stop; *next is where the following one starts */
static const char *lg__wrap(const char *s, float max_w, const char **next) {
    const char *p = s, *space = 0;
    float w = 0, sc = lg__fs();
    while (*p && *p != '\n') {
        unsigned int cp;
        const char *q = lg__utf8(p, p + 4, &cp);
        float gw = lg__adv(lg__glyph(cp), cp) * sc;
        if (w + gw > max_w && p > s) {
            if (space) { *next = space + 1; return space; }     /* break at the last space */
            *next = p;                                          /* one very long word: cut it */
            return p;
        }
        if (cp == ' ') space = p;
        w += gw;
        p = q;
    }
    *next = *p ? p + 1 : p;
    return p;
}
LGDEF void lg_text_wrapped(const char *text) {
    float max_w = lg__max(lg_available_width(), 20), lh = lg__line_h(), y;
    const char *s = text, *next, *e;
    int lines = 0;
    lg__rect r;
    do { lg__wrap(s, max_w, &next); lines++; s = next; } while (*s);
    r = lg__place(max_w, lh * lines);
    for (s = text, y = r.y; lines-- > 0; s = next, y += lh) {
        e = lg__wrap(s, max_w, &next);
        lg__text(r.x, y, s, e, lg__col(LG_COL_TEXT));
    }
}

/* ---------------------------------------------------------------- widgets */

/* text after a field or a slider: "the label" */
static void lg__label_after(lg__rect r, const char *label) {
    const char *e = lg__label_end(label);
    if (e > label) lg__text(r.x + r.w + lg__c.style.spacing_x, r.y + lg__c.style.padding_y, label, e, lg__col(LG_COL_TEXT));
}
/* fields leave room on the right for their label, when they have one */
static float lg__field_width(const char *label) {
    const char *e = lg__label_end(label);
    float avail = lg_available_width();
    if (lg__c.next_w != 0 || e == label) return lg__width(0);
    return lg__max(40, lg__min(avail * 0.65f, avail - lg__text_w(label, e) - lg__c.style.spacing_x));
}

LGDEF int lg_button(const char *label) {
    lg_style *s = &lg__c.style;
    const char *e = lg__label_end(label);
    float tw = lg__text_w(label, e);
    lg__rect r = lg__place(lg__width(tw + 2 * s->padding_x), lg__line_h() + 2 * s->padding_y);
    int hov, held, clicked = lg__widget(lg__id(label), r, &hov, &held);
    lg__fillr(r, lg__col(held && hov ? LG_COL_BUTTON_ACTIVE : hov ? LG_COL_BUTTON_HOVER : LG_COL_BUTTON));
    lg__push_clip(r);
    lg__text(r.x + lg__max(s->padding_x, (r.w - tw) / 2), r.y + s->padding_y, label, e, lg__col(LG_COL_TEXT));
    lg__pop_clip();
    lg__ring(r);
    return clicked;
}

LGDEF int lg_checkbox(const char *label, int *value) {
    lg_style *s = &lg__c.style;
    const char *e = lg__label_end(label);
    float size = lg__line_h(), rad = lg__min(s->rounding, 3);
    lg__rect r = lg__place(size + (e > label ? s->spacing_x + lg__text_w(label, e) : 0), size), box = lg__r(r.x, r.y, size, size);
    int hov, clicked = lg__widget(lg__id(label), r, &hov, 0);
    if (clicked) *value = !*value;
    lg__fill_round(box, lg__col(hov ? LG_COL_FRAME_HOVER : LG_COL_FRAME), rad, 15);
    lg__outline_round(box, s->border, lg__col(hov ? LG_COL_ACCENT : LG_COL_BORDER), rad, 15);
    if (*value) lg__icon(0x2713, box.x, box.y, size, lg__col(LG_COL_ACCENT));
    lg__text(r.x + size + s->spacing_x, r.y, label, e, lg__col(LG_COL_TEXT));
    lg__ring(box);
    return clicked;
}

static int lg__edit(unsigned int id, const char *label, char *buf, int cap, int flags, float pw, float ph, const lg__rect *at);

/* numbers to text and back, without <stdio.h> */
static int lg__itoa(unsigned int v, char *out) {
    char tmp[12];
    int n = 0, len = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v > 0);
    while (n > 0) out[len++] = tmp[--n];
    out[len] = 0;
    return len;
}
static int lg__ftoa(float v, int decimals, char *out) {
    unsigned int scale = 1, whole, i;
    double a = v < 0 ? -(double)v : (double)v;
    int n = 0, d;
    for (d = 0; d < decimals; d++) scale *= 10;
    a = a * scale + 0.5;
    if (!(a < 4e9)) a = 4e9;
    whole = (unsigned int)a;
    if (v < 0 && whole) out[n++] = '-';
    n += lg__itoa(whole / scale, out + n);
    if (decimals) {
        out[n++] = '.';
        for (i = scale / 10; i > 0; i /= 10) out[n++] = (char)('0' + whole / i % 10);
    }
    out[n] = 0;
    return n;
}
static float lg__atof(const char *s) {
    float v = 0, f = 0.1f;
    int neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    for (; *s >= '0' && *s <= '9'; s++) v = v * 10 + (float)(*s - '0');
    if (*s == '.' || *s == ',') for (s++; *s >= '0' && *s <= '9'; s++, f *= 0.1f) v += (float)(*s - '0') * f;
    return neg ? -v : v;
}

static int lg__slider(const char *label, float *value, float lo, float hi, int whole) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    lg__rect r = lg__place(lg__field_width(label), lg__line_h() + 2 * s->padding_y), knob;
    unsigned int id = lg__id(label);
    float grab = 8, in = 2, span = r.w - 2 * in - grab, t, old = *value, tw;
    int hov, held, clicked, n;
    char buf[32];

    if (c->num_id == id) {          /* the number is being typed: the slider is a text field for now */
        int fresh = c->num_frame == c->frame - 1, shown = 0, cancel = !c->num[0];
        if (fresh && c->focus == id) {
            lg__edit(id, label, c->num, (int)sizeof c->num, 0, 0, 0, &r);
            lg__label_after(r, label);
            c->num_frame = c->frame;
            if (c->focus == id) return 0;
            cancel = c->key_pressed[LG_KEY_ESCAPE] || !c->num[0];
            shown = 1;
        }
        c->num_id = 0;              /* Enter, Tab or a click somewhere else: the number is taken. Escape: it is not */
        if (fresh && !cancel) {
            float v = lg__atof(c->num);
            if (whole) v = lg__floor(v + 0.5f);
            *value = lo < hi ? lg__clamp(v, lo, hi) : lg__clamp(v, hi, lo);
        }
        if (shown) return *value != old;
    }

    clicked = lg__widget(id, r, &hov, &held);
    n = lg__ftoa(*value, whole ? 0 : 2, buf);
    tw = lg__text_w(buf, buf + n);
    /* a press on the number waits: a click types a number, a drag slides as usual */
    if (c->item_pressed) c->num_wait = lg__in(lg__r(r.x + (r.w - tw) / 2 - 4, r.y, tw + 8, r.h), c->mx, c->my) ? id : 0;
    if (c->num_wait == id && held && lg__far(3)) c->num_wait = 0;
    if (c->item_key || (clicked && c->num_wait == id)) {
        memcpy(c->num, buf, (size_t)n + 1);
        c->num_id = id; c->num_frame = c->frame; c->num_wait = 0;
        c->focus = id; c->focus_seen = c->focus_keep = 1;
        c->edit_multi = 0; c->edit_scroll = 0; c->sel = 0; c->caret = n;      /* everything selected: typing replaces it */
    } else if (held && c->num_wait != id && span > 0) {
        t = lg__clamp((c->mx - r.x - in - grab / 2) / span, 0, 1);
        *value = lo + t * (hi - lo);
        if (whole) *value = lg__floor(*value + 0.5f);
    }
    if (c->nav_show && c->nav_id == id) {       /* the arrow keys move it by a hundredth, or by one */
        float step = whole ? (hi > lo ? 1.0f : -1.0f) : (hi - lo) / 100;
        *value += step * (float)(c->key_pressed[LG_KEY_RIGHT] - c->key_pressed[LG_KEY_LEFT]);
        *value = lo < hi ? lg__clamp(*value, lo, hi) : lg__clamp(*value, hi, lo);
    }
    t = hi != lo ? lg__clamp((*value - lo) / (hi - lo), 0, 1) : 0;
    /* the handle stays inside the frame, so its corners never stick out of the frame's round ones */
    knob = lg__r(r.x + in + lg__floor(t * lg__max(span, 0)), r.y + in, grab, r.h - 2 * in);
    lg__fillr(r, lg__col(hov || held ? LG_COL_FRAME_HOVER : LG_COL_FRAME));
    lg__fill_round(lg__r(r.x, r.y, knob.x + grab / 2 - r.x, r.h), (lg__col(LG_COL_ACCENT) & 0xFFFFFF00u) | 0x50, s->rounding, 9);   /* the filled part, see-through */
    lg__fill_round(knob, lg__col(LG_COL_ACCENT), lg__max(0, s->rounding - in), 15);
    lg__outliner(r, s->border, lg__col(held ? LG_COL_ACCENT : LG_COL_BORDER));
    n = lg__ftoa(*value, whole ? 0 : 2, buf);
    lg__push_clip(r);
    lg__text(r.x + (r.w - lg__text_w(buf, buf + n)) / 2, r.y + s->padding_y, buf, buf + n, lg__col(LG_COL_TEXT));
    lg__pop_clip();
    lg__label_after(r, label);
    lg__ring(r);
    return *value != old;
}
LGDEF int lg_slider(const char *label, float *value, float lo, float hi) { return lg__slider(label, value, lo, hi, 0); }
LGDEF int lg_slider_int(const char *label, int *value, int lo, int hi) {
    float v = (float)*value;
    int changed = lg__slider(label, &v, (float)lo, (float)hi, 1);
    *value = (int)v;
    return changed;
}

LGDEF void lg_progress(float fraction, const char *text) {
    lg_style *s = &lg__c.style;
    lg__rect r = lg__place(lg__width(0), lg__line_h() + 2 * s->padding_y);
    lg__fillr(r, lg__col(LG_COL_FRAME));
    lg__fillr(lg__r(r.x, r.y, lg__floor(r.w * lg__clamp(fraction, 0, 1)), r.h), lg__col(LG_COL_ACCENT));
    lg__outliner(r, s->border, lg__col(LG_COL_BORDER));
    if (text) {
        lg__push_clip(r);
        lg__text(r.x + (r.w - lg_text_width(text)) / 2, r.y + s->padding_y, text, text + strlen(text), lg__col(LG_COL_TEXT));
        lg__pop_clip();
    }
}

LGDEF void lg_image(void *texture, float w, float h) {
    lg__rect r = lg__place(w, h);
    lg__quad(r.x, r.y, r.w, r.h, 0, 0, 1, 1, 0xFFFFFFFF, texture);
}

/* a full-width row: used by selectables, tree nodes and headers. natural: the width it really
   needs, which is what a popup sizes itself by */
static lg__rect lg__row(unsigned int id, float natural, int selected, int base_col, int *hov, int *clicked) {
    lg__layout *l = &lg__c.box->lay;
    lg__rect r;
    float max_x = l->max_x;
    l->same_line = 0;
    r = lg__place(l->x1 - l->x0 - l->indent, lg__line_h() + lg__c.style.padding_y);
    l->max_x = lg__max(max_x, r.x + natural);
    *clicked = lg__widget(id, r, hov, 0);
    if (base_col >= 0) lg__fillr(r, lg__col(base_col));
    if (selected) lg__fillr(r, lg__col(LG_COL_SELECTED));
    if (*hov) lg__fillr(r, lg__col(LG_COL_HOVER));
    return r;
}

LGDEF int lg_selectable(const char *label, int selected) {
    lg_style *s = &lg__c.style;
    const char *e = lg__label_end(label);
    int hov, clicked;
    lg__rect r = lg__row(lg__id(label), lg__text_w(label, e) + 2 * s->padding_x, selected, -1, &hov, &clicked);
    lg__push_clip(r);
    lg__text(r.x + s->padding_x, r.y + lg__floor(s->padding_y / 2), label, e, lg__col(LG_COL_TEXT));
    lg__pop_clip();
    lg__ring(r);
    if (clicked && lg__c.win && lg__c.win->is_popup) lg_close_popup();       /* picking from a menu closes it */
    return clicked;
}

LGDEF int lg_begin_menu_bar(void) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    lg__box *b;
    lg__rect strip;
    float h = lg__line_h() + 2 * s->padding_y;
    LOGUIA_ASSERT(!c->in_bar && "lg_begin_menu_bar inside a menu bar");
    c->main_bar = !c->box;
    if (c->main_bar) {          /* the main menu bar is a window that is nothing but its bar */
        lg_begin("##mainmenu", 0, 0, 0, h, LG_WINDOW_NO_TITLE | LG_WINDOW_NO_MOVE | LG_WINDOW_NO_RESIZE | LG_WINDOW_NO_SCROLL | LG_WINDOW_NO_BACKGROUND | LG__WINDOW_BAR);
        c->bar_frame = c->frame; c->bar_h = h;
    }
    b = c->box;
    LOGUIA_ASSERT(b->is_window && !b->is_popup && "a menu bar goes right after lg_begin, not into a panel or a popup");
    b->bar = c->frame;          /* the window keeps room for the bar from the next frame on */
    if (b->bar_h <= 0 || b->hidden) return 0;
    strip = lg__r(b->inner.x, b->bar_y, b->inner.w, b->bar_h);
    c->bar_lay = b->lay;
    lg__push_clip_raw(strip);
    lg__fill(strip, lg__col(LG_COL_TITLE));
    lg__fill(lg__r(strip.x, strip.y + strip.h - 1, strip.w, 1), lg__col(LG_COL_BORDER));
    memset(&b->lay, 0, sizeof b->lay);
    b->lay.x0 = b->lay.max_x = strip.x; b->lay.x1 = strip.x + strip.w;
    b->lay.cy = b->lay.max_y = strip.y;
    c->in_bar = 1;
    return 1;
}
LGDEF void lg_end_menu_bar(void) {
    lg__ctx *c = &lg__c;
    LOGUIA_ASSERT(c->in_bar && c->box && !c->box->is_popup && "lg_end_menu_bar without lg_begin_menu_bar (or a menu was left open)");
    c->box->lay = c->bar_lay;
    lg__pop_clip();
    c->in_bar = 0;
    if (c->main_bar) lg_end();
    c->main_bar = 0;
}
LGDEF int lg_begin_menu(const char *label) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    const char *e = lg__label_end(label);
    unsigned int id = lg__id(label), pid = lg__hash("#popup", 6, id);
    float tw = lg__text_w(label, e);
    int hov, clicked, open = c->npopups > c->pdepth && c->popups[c->pdepth] == pid;
    lg__rect r;
    LOGUIA_ASSERT(c->box && "menus go into a menu bar or a popup");
    if (c->in_bar && !c->box->is_popup) {           /* in the bar: a word that opens the menu under it */
        lg__layout *l = &c->box->lay;
        float gap = s->spacing_x;
        s->spacing_x = 0;
        if (l->has_item) l->same_line = 1;
        r = lg__place(tw + 2 * s->padding_x, c->box->bar_h - 1);
        s->spacing_x = gap;
        lg__widget(id, r, &hov, 0);
        if (hov || open) lg__fill(r, lg__col(open ? LG_COL_SELECTED : LG_COL_HOVER));
        lg__text(r.x + s->padding_x, r.y + s->padding_y, label, e, lg__col(LG_COL_TEXT));
        lg__ring(r);
        /* a press opens it (a press on an open one has just closed it); with a menu open, moving over the next word is enough */
        if (((c->item_pressed || c->item_key) && !open && c->popup_closed != pid) ||
            (hov && !open && c->npopups > c->pdepth && c->popups[c->pdepth] == c->menu_pid)) {
            lg__open_popup(pid, r.x, r.y + r.h + 1, 0);
            c->menu_pid = pid;
        }
    } else {                                        /* in a menu: a row with an arrow, the menu opens beside it */
        r = lg__row(id, tw + 2 * s->padding_x + lg__line_h(), open, -1, &hov, &clicked);
        lg__text(r.x + s->padding_x, r.y + lg__floor(s->padding_y / 2), label, e, lg__col(LG_COL_TEXT));
        lg__icon(0x25B6, r.x + r.w - r.h, r.y, r.h, lg__col(LG_COL_TEXT_DIM));
        lg__ring(r);
        if ((hov || clicked) && !open) lg__open_popup(pid, r.x + r.w + 2, r.y - s->padding_y - 2, 0);
    }
    return lg__begin_popup(pid);
}
LGDEF void lg_end_menu(void) { lg_end_popup(); }
LGDEF int lg_menu_item(const char *label, const char *shortcut, int checked) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    const char *e = lg__label_end(label);
    float sw = shortcut ? lg_text_width(shortcut) : 0, mark = lg__line_h(), x;
    int hov, clicked;
    lg__rect r = lg__row(lg__id(label), lg__text_w(label, e) + 2 * s->padding_x + (sw > 0 ? sw + 2 * s->padding_x : 0) + (checked ? mark : 0), 0, -1, &hov, &clicked);
    x = r.x + r.w - s->padding_x;
    lg__text(r.x + s->padding_x, r.y + lg__floor(s->padding_y / 2), label, e, lg__col(LG_COL_TEXT));
    if (checked) { x -= mark; lg__icon(0x2713, x, r.y + (r.h - mark) / 2, mark, lg__col(LG_COL_ACCENT)); }
    if (shortcut) lg__text(x - sw, r.y + lg__floor(s->padding_y / 2), shortcut, shortcut + strlen(shortcut), lg__col(LG_COL_TEXT_DIM));
    lg__ring(r);
    if (hov && c->npopups > c->pdepth) lg__close_popups(c->pdepth);     /* a menu that was open beside another row closes */
    if (clicked) lg__close_popups(0);                                   /* picking closes the whole menu */
    return clicked;
}

LGDEF int lg_tree_begin(const char *label, int flags) {
    lg__ctx *c = &lg__c;
    const char *e = lg__label_end(label);
    unsigned int id = lg__id(label);
    int leaf = flags & LG_TREE_LEAF, hov, clicked;
    int *open = lg__state(id, (flags & LG_TREE_DEFAULT_OPEN) != 0);
    float size = lg__line_h() + c->style.padding_y;
    lg__rect r = lg__row(id, size + 2 + lg__text_w(label, e) + c->style.padding_x, flags & LG_TREE_SELECTED, -1, &hov, &clicked);
    if (clicked && !leaf && (c->item_key || !(flags & LG_TREE_OPEN_ON_ARROW) || c->mx < r.x + size)) *open = !*open;
    if (!leaf && c->nav_show && c->nav_id == id) {      /* Right opens, Left closes */
        if (c->key_pressed[LG_KEY_RIGHT]) *open = 1;
        if (c->key_pressed[LG_KEY_LEFT]) *open = 0;
    }
    if (leaf) lg__fill(lg__r(r.x + lg__floor(size / 2) - 2, r.y + lg__floor(size / 2) - 2, 4, 4), lg__col(LG_COL_TEXT_DIM));
    else lg__icon(*open ? 0x25BC : 0x25B6, r.x, r.y, size, lg__col(LG_COL_TEXT_DIM));
    lg__push_clip(r);
    lg__text(r.x + size + 2, r.y + lg__floor(c->style.padding_y / 2), label, e, lg__col(LG_COL_TEXT));
    lg__pop_clip();
    lg__ring(r);
    if (leaf || !*open) return 0;
    lg_indent(0);
    lg__push(id);
    return 1;
}
LGDEF void lg_tree_end(void) { lg_unindent(0); lg_pop_id(); }

LGDEF int lg_header(const char *label, int default_open) {
    const char *e = lg__label_end(label);
    unsigned int id = lg__id(label);
    int hov, clicked, *open = lg__state(id, default_open);
    float size = lg__line_h() + lg__c.style.padding_y;
    lg__rect r = lg__row(id, size + 2 + lg__text_w(label, e) + lg__c.style.padding_x, 0, LG_COL_BUTTON, &hov, &clicked);
    if (clicked) *open = !*open;
    lg__icon(*open ? 0x25BC : 0x25B6, r.x, r.y, r.h, lg__col(LG_COL_TEXT));
    lg__push_clip(r);
    lg__text(r.x + r.h + 2, r.y + lg__floor(lg__c.style.padding_y / 2), label, e, lg__col(LG_COL_TEXT));
    lg__pop_clip();
    lg__ring(r);
    return *open;
}

LGDEF int lg_begin_combo(const char *label, const char *preview) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    unsigned int id = lg__id(label), pid = lg__hash("#popup", 6, id);
    lg__rect r = lg__place(lg__field_width(label), lg__line_h() + 2 * s->padding_y);
    int hov, open = c->npopups > c->pdepth && c->popups[c->pdepth] == pid, pressed;
    lg__widget(id, r, &hov, 0);
    pressed = c->item_pressed || c->item_key;
    lg__fillr(r, lg__col(hov || open ? LG_COL_FRAME_HOVER : LG_COL_FRAME));
    lg__outliner(r, s->border, lg__col(open ? LG_COL_ACCENT : LG_COL_BORDER));
    lg__push_clip(lg__r(r.x, r.y, r.w - r.h, r.h));
    lg__text(r.x + s->padding_x, r.y + s->padding_y, preview, preview + strlen(preview), lg__col(LG_COL_TEXT));
    lg__pop_clip();
    lg__icon(0x25BC, r.x + r.w - r.h, r.y, r.h, lg__col(LG_COL_TEXT_DIM));
    lg__label_after(r, label);
    lg__ring(r);
    /* a press on the field of an open list has just closed it (a click outside the popup): leave it closed */
    if (pressed && !open && c->popup_closed != pid) lg__open_popup(pid, r.x, r.y + r.h + 2, r.w);
    return lg__begin_popup(pid);
}
LGDEF int lg_combo(const char *label, int *current, const char *const *items, int count) {
    int i, changed = 0;
    if (!lg_begin_combo(label, *current >= 0 && *current < count ? items[*current] : "")) return 0;
    for (i = 0; i < count; i++) {
        lg_push_id(i);
        if (lg_selectable(items[i], i == *current) && i != *current) { *current = i; changed = 1; }
        lg_pop_id();
    }
    lg_end_popup();
    return changed;
}

LGDEF int lg_tabs(const char *name, const char *const *labels, int count, int *current) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    lg__layout *l = &c->box->lay;
    float gap = s->spacing_x, h = lg__line_h() + 2 * s->padding_y;
    int i, changed = 0, hov;
    lg_push_id_str(name);
    l->same_line = 0;
    s->spacing_x = 2;
    for (i = 0; i < count; i++) {
        const char *e = lg__label_end(labels[i]);
        float tw = lg__text_w(labels[i], e), w = tw + 2 * s->padding_x;
        int on = i == *current;
        lg__rect r;
        if (i && l->line_x + s->spacing_x + w <= l->x1) l->same_line = 1;       /* a full row continues below */
        r = lg__place(w, h);
        lg_push_id(i);
        if (lg__widget(lg__id(labels[i]), r, &hov, 0) && !on) { *current = i; changed = on = 1; }
        lg_pop_id();
        lg__fill_round(r, lg__col(on ? LG_COL_BUTTON : hov ? LG_COL_BUTTON_HOVER : LG_COL_FRAME), s->rounding, 3);
        if (on) lg__fill(lg__r(r.x, r.y + r.h - 2, r.w, 2), lg__col(LG_COL_ACCENT));
        lg__text(r.x + s->padding_x, r.y + s->padding_y, labels[i], e, lg__col(on ? LG_COL_TEXT : LG_COL_TEXT_DIM));
        lg__ring(r);
    }
    s->spacing_x = gap;
    l->cy -= s->spacing_y;          /* the line sits right under the tabs */
    lg_separator();
    lg_pop_id();
    return changed;
}

/* ---------------------------------------------------------------- text fields */

static int lg__prev_char(const char *s, int i) { do i--; while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80); return i < 0 ? 0 : i; }
static int lg__next_char(const char *s, int i, int len) { if (i < len) i++; while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80) i++; return i; }
static int lg__line_start(const char *s, int i) { while (i > 0 && s[i - 1] != '\n') i--; return i; }
static int lg__line_end(const char *s, int i, int len) { while (i < len && s[i] != '\n') i++; return i; }

/* the caret position nearest to x pixels from the start of the text */
static int lg__caret_at(const char *s, int len, float x) {
    int i = 0;
    float w = 0;
    while (i < len) {
        int n = lg__next_char(s, i, len);
        float cw = lg__text_w(s + i, s + n);
        if (x < w + cw / 2) break;
        w += cw;
        i = n;
    }
    return i;
}

/* Text is shown as rows. A row is a line, or a piece of a line when long lines are folded.
   lg__row: where the row starting at a ends; *next is where the following row starts.
   wrap: above 0 folds lines at that width, 0 never folds, below 0 means one-line text. */
static int lg__trow(const char *s, int a, int len, float wrap, int *next) {
    int e;
    if (wrap < 0) { *next = len; return len; }
    if (wrap > 0) {
        const char *n;
        e = (int)(lg__wrap(s + a, wrap, &n) - s);
        *next = (int)(n - s);
        return e;
    }
    e = lg__line_end(s, a, len);
    *next = e < len ? e + 1 : len;
    return e;
}
/* row number `row` (or the last one, when there are fewer): where it starts and ends */
static void lg__row_at(const char *s, int len, float wrap, int row, int *a, int *e) {
    int next;
    for (*a = 0;; *a = next) {
        *e = lg__trow(s, *a, len, wrap, &next);
        if (row-- <= 0 || *e >= len) return;
    }
}
/* the number of the row the caret is on, and where that row starts and ends */
static int lg__row_of(const char *s, int len, float wrap, int caret, int *a, int *e) {
    int next, row = 0;
    for (*a = 0;; *a = next, row++) {
        *e = lg__trow(s, *a, len, wrap, &next);
        if (caret < next || *e >= len) return row;
    }
}
/* the caret position nearest to the point (x, y), measured from the top-left corner of the text */
static int lg__caret_xy(const char *s, int len, float wrap, float x, float y) {
    int a, e;
    lg__row_at(s, len, wrap, wrap < 0 ? 0 : (int)lg__floor(y / lg__line_h()), &a, &e);
    return a + lg__caret_at(s + a, e - a, x);
}
static int lg__delete_selection(char *buf, int len) {
    lg__ctx *c = &lg__c;
    int a = c->caret < c->sel ? c->caret : c->sel, b = c->caret < c->sel ? c->sel : c->caret;
    if (a == b) return 0;
    memmove(buf + a, buf + b, (size_t)(len - b + 1));
    c->caret = c->sel = a;
    return 1;
}
static int lg__insert(char *buf, int cap, const char *text, int n) {
    lg__ctx *c = &lg__c;
    int len = (int)strlen(buf);
    while (n > 0 && len + n + 1 > cap) n = lg__prev_char(text, n);     /* keep only what fits, whole characters */
    if (n <= 0) return 0;
    memmove(buf + c->caret + n, buf + c->caret, (size_t)(len - c->caret + 1));
    memcpy(buf + c->caret, text, (size_t)n);
    c->caret = c->sel = c->caret + n;
    return 1;
}
/* replaces the selection */
static int lg__type(char *buf, int cap, const char *text, int n) {
    int changed = lg__delete_selection(buf, (int)strlen(buf));
    return lg__insert(buf, cap, text, n) | changed;
}
/* Undo: before a change the whole text is put aside, and Ctrl+Z brings it back. Only the field
   that is being edited has a history. */
static void lg__snap_clear(lg__snap *s, int *n) { while (*n > 0) LOGUIA_FREE(s[--*n].text); }
static void lg__snap_push(lg__snap *s, int *n, const char *buf) {
    size_t len = strlen(buf);
    if (*n == LOGUIA_UNDO_LEVELS) {         /* full: the oldest one goes */
        LOGUIA_FREE(s[0].text);
        memmove(s, s + 1, (LOGUIA_UNDO_LEVELS - 1) * sizeof *s);
        --*n;
    }
    s[*n].text = (char *)LOGUIA_MALLOC(len + 1);
    memcpy(s[*n].text, buf, len + 1);
    s[*n].caret = lg__c.caret; s[*n].sel = lg__c.sel;
    ++*n;
}
static void lg__snap_pop(lg__snap *s, int *n, char *buf, int cap) {
    lg__snap *t = &s[--*n];
    int len = (int)strlen(t->text);
    if (len >= cap) len = cap - 1;
    memcpy(buf, t->text, (size_t)len);
    buf[len] = 0;
    lg__c.caret = t->caret < len ? t->caret : len;
    lg__c.sel = t->sel < len ? t->sel : len;
    LOGUIA_FREE(t->text);
}

#define LG__EDIT_MULTI 0x100

/* One-line fields and many-line editors are the same thing; the editor sits in a scrolling panel.
   at: a one-line field exactly there, taking no room in the layout (a slider being typed into). */
static int lg__edit(unsigned int id, const char *label, char *buf, int cap, int flags, float pw, float ph, const lg__rect *at) {
    lg__ctx *c = &lg__c;
    lg_style *s = &c->style;
    lg__box *b = 0;
    lg__rect r, in, cl;
    int multi = flags & LG__EDIT_MULTI, fixed = flags & LG_EDIT_READ_ONLY;
    int len = (int)strlen(buf), hov, held, result = 0, focused, i, a, e, next, sa, sb, rows = 1, caret0, by_key, page = 1;
    int kind = 0, pushed = 0, lines = 1, line, caret_row;
    int shift = c->key_down[LG_KEY_SHIFT];
    const unsigned char *ctrl = c->key_ctrl, *sh = c->key_shift;
    float lh = lg__line_h(), scroll, max_w = 0, view = 0, y, wrap = -1, gutter = 0;

    if (multi) {
        b = lg__panel(lg__hash("#edit", 5, id), pw, ph, 0, s->padding_x, c->focus == id ? LG_COL_FRAME_HOVER : LG_COL_FRAME,
                      c->focus == id ? LG_COL_ACCENT : LG_COL_BORDER);
        if (!(flags & LG_EDIT_NO_LINE_NUMBERS)) {       /* room for the line numbers: at least two digits */
            int digits = 2, n;
            for (i = 0; i < len; i++) lines += buf[i] == '\n';
            for (n = lines; n >= 100; n /= 10) digits++;
            gutter = (float)digits * lg__text_w("0", &"0"[1]) + 2 * s->padding_x;
        }
        r = lg__r(b->inner.x, b->inner.y, b->lay.x1 + b->pad - b->inner.x, b->inner.h);     /* all but the scrollbar */
        in = lg__r(b->lay.x0 + gutter, b->lay.cy, lg__max(1, b->lay.x1 - b->lay.x0 - gutter), lh);
        view = b->inner.h - 2 * b->pad;
        page = (int)(view / lh) > 1 ? (int)(view / lh) - 1 : 1;
        wrap = (flags & LG_EDIT_WRAP) ? in.w : 0;
    } else {
        r = at ? *at : lg__place(lg__field_width(label), lh + 2 * s->padding_y);
        in = lg__r(r.x + s->padding_x, r.y + s->padding_y, r.w - 2 * s->padding_x, lh);
    }

    lg__widget(id, r, &hov, &held);
    by_key = c->item_key;
    if (c->nav_arrived == id || by_key) {       /* the keyboard came here: start typing */
        c->nav_arrived = 0;
        c->focus = id; c->edit_multi = multi; c->edit_scroll = 0;
        c->sel = 0; c->caret = multi ? 0 : len;
    }
    if (c->item_pressed) {
        int twice = c->click_id == id && c->time - c->click_time < 0.35;
        if (c->focus != id) { c->focus = id; c->edit_scroll = 0; }
        c->edit_multi = multi;
        c->caret = lg__caret_xy(buf, len, wrap, c->mx - in.x + c->edit_scroll, c->my - in.y);
        if (!shift) c->sel = c->caret;
        if (twice) { c->sel = lg__line_start(buf, c->caret); c->caret = lg__line_end(buf, c->caret, len); }   /* a double click takes the line */
        c->click_id = twice ? 0 : id;
        c->click_time = c->time;
        c->focus_keep = 1;
    } else if (held && c->focus == id && (c->mx != c->pmx || c->my != c->pmy)) {
        c->caret = lg__caret_xy(buf, len, wrap, c->mx - in.x + c->edit_scroll, c->my - in.y);     /* dragging selects */
    }
    if (c->focus == id && c->nav_show && c->nav_id != id) c->focus = 0;     /* Tab went on to another widget */
    focused = c->focus == id;
    caret0 = c->caret;

    if (focused) {
        c->focus_seen = 1;
        if (c->caret > len) c->caret = len;
        if (c->sel > len) c->sel = len;
        if (c->undo_id != id) {             /* another field: its history starts empty */
            lg__snap_clear(c->undo, &c->nundo); lg__snap_clear(c->redo, &c->nredo);
            c->undo_id = id; c->undo_kind = 0;
        }
        /* what is about to change the text? typing joins into one step, anything else is a step of its own */
        if (!fixed) {
            if (c->nchars) kind = 1;
            if (c->key_pressed[LG_KEY_BACKSPACE] || c->key_pressed[LG_KEY_DELETE] || ctrl[LG_KEY_X] || ctrl[LG_KEY_V] ||
                (multi && !by_key && (c->key_pressed[LG_KEY_ENTER] || c->key_pressed[LG_KEY_TAB]))) kind = 2;
            if (kind && !(kind == 1 && c->undo_kind == 1 && c->nundo && c->time - c->undo_time < 1.0)) { lg__snap_push(c->undo, &c->nundo, buf); pushed = 1; }
        }
        for (i = 0; i < c->nchars && !fixed; i++) {
            char u[4];
            result |= lg__type(buf, cap, u, lg__utf8_put(c->chars[i], u));
        }
        if (multi && !by_key && !fixed) {
            for (i = c->key_pressed[LG_KEY_ENTER]; i > 0; i--) result |= lg__type(buf, cap, "\n", 1);
            for (i = c->key_pressed[LG_KEY_TAB]; i > 0; i--) result |= lg__type(buf, cap, "    ", 4);
        }
        len = (int)strlen(buf);
        for (i = c->key_pressed[LG_KEY_LEFT]; i > 0; i--) {
            if (!sh[LG_KEY_LEFT] && c->sel != c->caret) c->caret = c->caret < c->sel ? c->caret : c->sel;
            else c->caret = lg__prev_char(buf, c->caret);
            if (!sh[LG_KEY_LEFT]) c->sel = c->caret;
        }
        for (i = c->key_pressed[LG_KEY_RIGHT]; i > 0; i--) {
            if (!sh[LG_KEY_RIGHT] && c->sel != c->caret) c->caret = c->caret > c->sel ? c->caret : c->sel;
            else c->caret = lg__next_char(buf, c->caret, len);
            if (!sh[LG_KEY_RIGHT]) c->sel = c->caret;
        }
        if (multi) {        /* up and down keep the caret at the same distance from the left */
            for (i = c->key_pressed[LG_KEY_UP] + page * c->key_pressed[LG_KEY_PAGE_UP]; i > 0; i--) {
                int row = lg__row_of(buf, len, wrap, c->caret, &a, &e);
                float x = lg__text_w(buf + a, buf + c->caret);
                if (row == 0) c->caret = 0;
                else { lg__row_at(buf, len, wrap, row - 1, &a, &e); c->caret = a + lg__caret_at(buf + a, e - a, x); }
                if (!sh[LG_KEY_UP] && !sh[LG_KEY_PAGE_UP]) c->sel = c->caret;
            }
            for (i = c->key_pressed[LG_KEY_DOWN] + page * c->key_pressed[LG_KEY_PAGE_DOWN]; i > 0; i--) {
                int row = lg__row_of(buf, len, wrap, c->caret, &a, &e);
                float x = lg__text_w(buf + a, buf + c->caret);
                if (e >= len) c->caret = len;
                else { lg__row_at(buf, len, wrap, row + 1, &a, &e); c->caret = a + lg__caret_at(buf + a, e - a, x); }
                if (!sh[LG_KEY_DOWN] && !sh[LG_KEY_PAGE_DOWN]) c->sel = c->caret;
            }
        }
        if (c->key_pressed[LG_KEY_HOME]) { lg__row_of(buf, len, wrap, c->caret, &a, &e); c->caret = ctrl[LG_KEY_HOME] ? 0 : a; if (!sh[LG_KEY_HOME]) c->sel = c->caret; }
        if (c->key_pressed[LG_KEY_END]) { lg__row_of(buf, len, wrap, c->caret, &a, &e); c->caret = ctrl[LG_KEY_END] ? len : e; if (!sh[LG_KEY_END]) c->sel = c->caret; }
        for (i = c->key_pressed[LG_KEY_BACKSPACE]; i > 0 && !fixed; i--) {
            if (c->sel == c->caret) c->sel = lg__prev_char(buf, c->caret);
            result |= lg__delete_selection(buf, (int)strlen(buf));
        }
        for (i = c->key_pressed[LG_KEY_DELETE]; i > 0 && !fixed; i--) {
            if (c->sel == c->caret) c->sel = lg__next_char(buf, c->caret, (int)strlen(buf));
            result |= lg__delete_selection(buf, (int)strlen(buf));
        }
        len = (int)strlen(buf);
        if (ctrl[LG_KEY_A]) { c->sel = 0; c->caret = len; }
        if ((ctrl[LG_KEY_C] || ctrl[LG_KEY_X]) && c->clip_set && c->sel != c->caret) {
            char keep;
            a = c->caret < c->sel ? c->caret : c->sel; e = c->caret < c->sel ? c->sel : c->caret;
            keep = buf[e];
            buf[e] = 0;
            c->clip_set(c->clip_user, buf + a);
            buf[e] = keep;
            if (ctrl[LG_KEY_X] && !fixed) result |= lg__delete_selection(buf, len);
        }
        if (ctrl[LG_KEY_V] && c->clip_get && !fixed) {
            const char *t = c->clip_get(c->clip_user);
            if (t && *t) {
                result |= lg__delete_selection(buf, (int)strlen(buf));
                for (;;) {          /* line ends from Windows lose their '\r'; a one-line field takes the first line only */
                    int n = 0;
                    while (t[n] && t[n] != '\r' && (multi || t[n] != '\n')) n++;
                    result |= lg__insert(buf, cap, t, n);
                    if (!multi || t[n] != '\r') break;
                    t += n + 1;
                }
            }
        }
        if (kind && result) { c->undo_kind = kind; c->undo_time = c->time; lg__snap_clear(c->redo, &c->nredo); }
        else if (pushed) LOGUIA_FREE(c->undo[--c->nundo].text);         /* nothing changed after all */
        /* Ctrl+Z takes a step back, Ctrl+Y or Ctrl+Shift+Z forward again */
        if (!fixed && ctrl[LG_KEY_Z] && !sh[LG_KEY_Z] && c->nundo) {
            lg__snap_push(c->redo, &c->nredo, buf);
            lg__snap_pop(c->undo, &c->nundo, buf, cap);
            c->undo_kind = 0; result |= LG_INPUT_CHANGED;
        } else if (!fixed && (ctrl[LG_KEY_Y] || (ctrl[LG_KEY_Z] && sh[LG_KEY_Z])) && c->nredo) {
            lg__snap_push(c->undo, &c->nundo, buf);
            lg__snap_pop(c->redo, &c->nredo, buf, cap);
            c->undo_kind = 0; result |= LG_INPUT_CHANGED;
        }
        len = (int)strlen(buf);
        if (!multi && !by_key && c->key_pressed[LG_KEY_ENTER] && !c->compose[0]) { result |= LG_INPUT_ENTER; c->focus = 0; }
        if (c->key_pressed[LG_KEY_ESCAPE]) c->focus = 0;
    }

    /* how big the text is */
    for (a = 0;; a = next) {
        e = lg__trow(buf, a, len, wrap, &next);
        max_w = lg__max(max_w, lg__text_w(buf + a, buf + e));
        if (e >= len) break;
        rows++;
    }
    if (multi) lg__place(in.w + gutter, lh * (float)rows);
    caret_row = lg__row_of(buf, len, wrap, c->caret < len ? c->caret : len, &a, &e);
    if (focused) {      /* keep the caret in view */
        float cx = lg__text_w(buf + a, buf + c->caret);
        if (cx - c->edit_scroll > in.w - 2) c->edit_scroll = cx - in.w + 2;
        if (cx - c->edit_scroll < 0) c->edit_scroll = cx;
        c->edit_scroll = wrap > 0 ? 0 : lg__max(0, lg__min(c->edit_scroll, lg__max(0, max_w - in.w + 2)));
        if (multi && (c->caret != caret0 || result)) {
            float cy = lh * (float)caret_row;
            if (cy < b->scroll) b->scroll = cy;
            else if (cy + lh > b->scroll + view) b->scroll = cy + lh - view;
        }
    }
    scroll = focused ? lg__floor(c->edit_scroll) : 0;

    if (!multi) {
        lg__fillr(r, lg__col(hov || focused ? LG_COL_FRAME_HOVER : LG_COL_FRAME));
        lg__outliner(r, s->border, lg__col(focused ? LG_COL_ACCENT : LG_COL_BORDER));
    }
    cl = lg__clip();
    if (gutter > 0) {       /* line numbers: one for every line, not for the folded pieces of a line */
        float gx = b->lay.x0 + gutter - s->padding_x;
        for (a = 0, y = in.y, line = 0, i = 0;; a = next, y += lh, i++) {
            e = lg__trow(buf, a, len, wrap, &next);
            if (a == 0 || buf[a - 1] == '\n') {
                line++;
                if (y + lh > cl.y && y < cl.y + cl.h) {
                    char num[12];
                    int n = lg__itoa((unsigned int)line, num), here = focused && c->caret >= a && c->caret <= lg__line_end(buf, a, len);
                    lg__text(gx - s->padding_x - lg__text_w(num, num + n), y, num, num + n, lg__col(here ? LG_COL_TEXT : LG_COL_TEXT_DIM));
                }
            }
            if (e >= len) break;
        }
        lg__fill(lg__r(gx, b->inner.y, 1, b->inner.h), lg__col(LG_COL_SEPARATOR));
    }
    lg__push_clip(lg__r(in.x, r.y, in.w, r.h));
    cl = lg__clip();
    sa = c->caret < c->sel ? c->caret : c->sel; sb = c->caret < c->sel ? c->sel : c->caret;
    for (a = 0, y = in.y, i = 0;; a = next, y += lh, i++) {
        e = lg__trow(buf, a, len, wrap, &next);
        if (y + lh > cl.y && y < cl.y + cl.h) {         /* only the rows that can be seen */
            if (focused && sa != sb && sa <= e && sb >= a) {
                float x0 = lg__text_w(buf + a, buf + (sa > a ? sa : a)), x1 = lg__text_w(buf + a, buf + (sb < e ? sb : e));
                if (sb > e) x1 += lg__text_w(" ", &" "[1]) / 2;     /* the line end is selected too */
                lg__fill(lg__r(in.x - scroll + x0, y, x1 - x0, lh), lg__col(LG_COL_TEXT_SELECT));
            }
            lg__text(in.x - scroll, y, buf + a, buf + e, lg__col(LG_COL_TEXT));
            if (focused && i == caret_row) {
                float cx = in.x - scroll + lg__text_w(buf + a, buf + c->caret);
                c->ime = lg__r(cx, y, 1, lh);           /* where the system puts its list of candidates */
                c->ime_frame = c->frame;
                if (((int)(c->time * 2) & 1) == 0 && !c->compose[0]) lg__fill(lg__r(cx, y, 1, lh), lg__col(LG_COL_TEXT));
            }
        }
        if (e >= len) break;
    }
    lg__pop_clip();
    if (focused && c->compose[0] && c->ime_frame == c->frame) {      /* the word an input method is composing, at the caret */
        const char *t = c->compose, *te = t + strlen(t);
        int layer = c->layer;
        float tw = lg__text_w(t, te);
        lg__rect box = lg__r(c->ime.x - 2, c->ime.y - 1, tw + 4, lh + 2);
        c->layer = 0x7FFFFFE8;
        lg__push_clip_raw(lg__r(0, 0, c->w, c->h));
        lg__fill(box, lg__col(LG_COL_FRAME_HOVER));
        lg__text(c->ime.x, c->ime.y, t, te, lg__col(LG_COL_TEXT));
        lg__fill(lg__r(c->ime.x, c->ime.y + lh - 1, tw, 1), lg__col(LG_COL_ACCENT));     /* underlined, as composing text is */
        if (((int)(c->time * 2) & 1) == 0) lg__fill(lg__r(c->ime.x + lg__text_w(t, t + c->compose_caret), c->ime.y, 1, lh), lg__col(LG_COL_TEXT));
        lg__pop_clip();
        c->layer = layer;
    }
    if (multi) {
        lg_end_panel();
        c->item_rect = b->rect;
        c->item_hovered = hov;
    } else if (!at) {
        lg__label_after(r, label);
    }
    c->item_id = id;
    lg__ring(multi ? b->rect : r);
    return result;
}
LGDEF int lg_input_text(const char *label, char *buf, int cap) { return lg__edit(lg__id(label), label, buf, cap, 0, 0, 0, 0); }
LGDEF int lg_input_text_multiline(const char *label, char *buf, int cap, float w, float h, int flags) {
    return lg__edit(lg__id(label), label, buf, cap, (flags & 0xFF) | LG__EDIT_MULTI, w, h, 0);
}

/* ---------------------------------------------------------------- the docked layout as text */

/* One line per place ("N") and per window ("W"), each a list of whole numbers. */
static void lg__save_num(char *buf, int cap, int *n, int negative, unsigned int v, char end) {
    char t[12];
    int i, len = lg__itoa(v, t);
    if (negative) { if (*n < cap - 1) buf[*n] = '-'; ++*n; }
    for (i = 0; i < len; i++, ++*n) if (*n < cap - 1) buf[*n] = t[i];
    if (*n < cap - 1) buf[*n] = end;
    ++*n;
}
static void lg__save_int(char *buf, int cap, int *n, float v, char end) {
    int i = (int)lg__floor(v + 0.5f);
    lg__save_num(buf, cap, n, i < 0, i < 0 ? (unsigned int)-i : (unsigned int)i, end);
}
LGDEF int lg_dock_save(char *buf, int cap) {
    lg__ctx *c = &lg__c;
    int n = 0, i;
    for (i = 0; i < LOGUIA_MAX_DOCKS; i++) {
        lg__dock *d = &c->docks[i];
        if (d->kind == LG__D_FREE) continue;
        if (n < cap - 1) buf[n] = 'N';
        n++;
        lg__save_int(buf, cap, &n, (float)i, ' ');          lg__save_int(buf, cap, &n, (float)d->kind, ' ');
        lg__save_int(buf, cap, &n, (float)d->a, ' ');       lg__save_int(buf, cap, &n, (float)d->b, ' ');
        lg__save_int(buf, cap, &n, d->share * 10000, ' ');  lg__save_int(buf, cap, &n, (float)d->centre, ' ');
        lg__save_num(buf, cap, &n, 0, d->tab, ' ');         lg__save_int(buf, cap, &n, (float)(d->parent < 0 && i), ' ');
        lg__save_int(buf, cap, &n, d->fr.x, ' ');           lg__save_int(buf, cap, &n, d->fr.y, ' ');
        lg__save_int(buf, cap, &n, d->fr.w, ' ');           lg__save_int(buf, cap, &n, d->fr.h, '\n');
    }
    for (i = 0; i < LOGUIA_MAX_BOXES; i++) {
        lg__box *b = &c->boxes[i];
        if (!b->id || !b->is_window || b->is_popup || !b->placed || (b->flags & LG__WINDOW_BAR)) continue;
        if (n < cap - 1) buf[n] = 'W';
        n++;
        lg__save_num(buf, cap, &n, 0, b->id, ' ');          lg__save_int(buf, cap, &n, (float)b->dock, ' ');
        lg__save_int(buf, cap, &n, (float)b->order, ' ');   lg__save_int(buf, cap, &n, b->fw, ' ');
        lg__save_int(buf, cap, &n, b->fh, ' ');             lg__save_int(buf, cap, &n, b->rect.x, ' ');
        lg__save_int(buf, cap, &n, b->rect.y, ' ');         lg__save_int(buf, cap, &n, b->dock ? b->fw : b->rect.w, ' ');
        lg__save_int(buf, cap, &n, b->dock ? b->fh : b->rect.h, '\n');
    }
    if (cap > 0) buf[n < cap ? n : cap - 1] = 0;
    return n;
}
static void lg__dock_reset(void) {
    lg__ctx *c = &lg__c;
    int i;
    for (i = 0; i < LOGUIA_MAX_BOXES; i++) if (c->boxes[i].dock) lg__undock(&c->boxes[i]);
    memset(c->docks, 0, sizeof c->docks);
    c->docks[0].kind = LG__D_HOLE; c->docks[0].parent = -1;
    c->drop_side = 0;
}
LGDEF void lg_dock_load(const char *text) {
    lg__ctx *c = &lg__c;
    const char *s = text;
    unsigned char seen[LOGUIA_MAX_DOCKS];
    int i, k, ok = 1, floats[LOGUIA_MAX_DOCKS];
    lg__dock_reset();
    memset(floats, 0, sizeof floats);
    while (*s) {
        double v[12];
        int count = 0, tag = *s++;
        memset(v, 0, sizeof v);
        if (tag == '\n') continue;
        while (*s && *s != '\n') {
            double x = 0;
            int neg = 0;
            if (*s == '-' && s[1] >= '0' && s[1] <= '9') { neg = 1; s++; }
            if (*s < '0' || *s > '9') { s++; continue; }
            for (; *s >= '0' && *s <= '9'; s++) x = x * 10 + (*s - '0');
            if (count < 12) v[count++] = neg ? -x : x;
        }
        if (*s) s++;
        if (tag == 'N' && count >= 12 && v[0] >= 0 && v[0] < LOGUIA_MAX_DOCKS && v[1] >= LG__D_HOLE && v[1] <= LG__D_ROWS) {
            lg__dock *d = &c->docks[(int)v[0]];
            memset(d, 0, sizeof *d);
            d->kind = (int)v[1]; d->parent = -1;
            d->a = v[2] > 0 && v[2] < LOGUIA_MAX_DOCKS ? (int)v[2] : 0; d->b = v[3] > 0 && v[3] < LOGUIA_MAX_DOCKS ? (int)v[3] : 0;
            d->share = d->kind >= LG__D_COLS ? lg__clamp((float)v[4] / 10000, 0.02f, 0.98f) : 0; d->centre = v[5] != 0;
            d->tab = v[6] > 0 && v[6] < 4294967296.0 ? (unsigned int)v[6] : 0;
            floats[(int)v[0]] = v[7] != 0;
            d->fr = lg__r((float)v[8], (float)v[9], (float)v[10], (float)v[11]);
        } else if (tag == 'W' && count >= 9 && v[0] >= 1 && v[0] < 4294967296.0) {
            int is_new;
            lg__box *b = lg__get_box((unsigned int)v[0], &is_new);
            if (is_new) { b->is_window = 1; b->z = ++c->top_z; b->last_frame = c->frame; }
            if (!b->is_window || b->is_popup) continue;
            b->placed = 1;
            b->dock = v[1] >= 1 && v[1] <= LOGUIA_MAX_DOCKS ? (int)v[1] : 0;
            b->order = (int)v[2];
            if (b->order > c->dock_order) c->dock_order = b->order;
            b->fw = (float)v[3]; b->fh = (float)v[4];
            b->rect = lg__r((float)v[5], (float)v[6], (float)v[7], (float)v[8]);
        }
    }
    /* a text that was damaged must not leave a tree with loops or loose ends: every place has to hang, once, under a top */
    if (c->docks[0].kind == LG__D_FREE) ok = 0;
    for (i = 0; i < LOGUIA_MAX_DOCKS && ok; i++) {
        lg__dock *d = &c->docks[i];
        if (d->kind < LG__D_COLS) continue;
        if (!d->a || !d->b || d->a == d->b || c->docks[d->a].kind == LG__D_FREE || c->docks[d->b].kind == LG__D_FREE ||
            c->docks[d->a].parent >= 0 || c->docks[d->b].parent >= 0) { ok = 0; break; }
        c->docks[d->a].parent = c->docks[d->b].parent = i;
    }
    for (i = 0; i < LOGUIA_MAX_DOCKS; i++) {
        seen[i] = c->docks[i].kind != LG__D_FREE && c->docks[i].parent < 0;
        if (i && seen[i] && !floats[i]) ok = 0;         /* only groups that float may be a top besides the screen */
        if (seen[i] && i) c->docks[i].z = ++c->top_z;
    }
    for (k = 0; k < LOGUIA_MAX_DOCKS && ok; k++)
        for (i = 0; i < LOGUIA_MAX_DOCKS; i++)
            if (seen[i] && c->docks[i].kind >= LG__D_COLS) seen[c->docks[i].a] = seen[c->docks[i].b] = 1;
    for (i = 0; i < LOGUIA_MAX_DOCKS; i++) if (c->docks[i].kind != LG__D_FREE && !seen[i]) ok = 0;
    if (!ok) lg__dock_reset();
    for (i = 0; i < LOGUIA_MAX_BOXES; i++)
        if (c->boxes[i].dock && c->docks[c->boxes[i].dock - 1].kind != LG__D_LEAF) lg__undock(&c->boxes[i]);
    lg__dock_layout();
}

/* ======================================================================================
   LOGUIA_SOFTWARE: draw into a pixel buffer
   ====================================================================================== */
#ifdef LOGUIA_SOFTWARE
static float lg__edge(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
/* a pixel exactly on an edge belongs to only one of the two triangles sharing it */
static int lg__owns(float ax, float ay, float bx, float by) { return (ay == by && bx < ax) || by < ay; }
/* how solid the font picture is at (u, v), counted in its pixels; blends the four nearest */
static float lg__soft_font(const lg_font *f, float u, float v) {
    float fx = u - 0.5f, fy = v - 0.5f, top, bottom;
    int x0 = (int)lg__floor(fx), y0 = (int)lg__floor(fy), x1, y1;
    fx -= (float)x0; fy -= (float)y0;
    x1 = x0 + 1; y1 = y0 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= f->width) x1 = f->width - 1;
    if (y1 >= f->height) y1 = f->height - 1;
    if (x0 > x1) x0 = x1;
    if (y0 > y1) y0 = y1;
    top = f->pixels[y0 * f->width + x0] * (1 - fx) + f->pixels[y0 * f->width + x1] * fx;
    bottom = f->pixels[y1 * f->width + x0] * (1 - fx) + f->pixels[y1 * f->width + x1] * fx;
    return (top * (1 - fy) + bottom * fy) / 255.0f;
}
static void lg__soft_blend(unsigned char *p, float r, float g, float b, float al) {
    p[0] = (unsigned char)(p[0] + (r - p[0]) * al);
    p[1] = (unsigned char)(p[1] + (g - p[1]) * al);
    p[2] = (unsigned char)(p[2] + (b - p[2]) * al);
    p[3] = (unsigned char)(p[3] + (255 - p[3]) * al);
}
/* the first pixel whose middle is at or after v */
static int lg__soft_first(float v) { return -(int)lg__floor(0.5f - v); }

LGDEF void lg_soft_render(unsigned char *rgba, int w, int h) {
    const lg_draw_list *d = lg_get_draw_list();
    const lg_font *f = &lg__c.font;
    float sx = lg__c.w > 0 ? (float)w / lg__c.w : 1, sy = lg__c.h > 0 ? (float)h / lg__c.h : 1;
    float fw = (float)f->width, fh = (float)f->height;
    int ci, x, y;
    unsigned int k;
    for (ci = 0; ci < d->cmd_count; ci++) {
        const lg_cmd *cmd = &d->cmds[ci];
        int cx0 = lg__soft_first(lg__max(0, cmd->clip_x * sx)), cy0 = lg__soft_first(lg__max(0, cmd->clip_y * sy));
        int cx1 = lg__soft_first(lg__min((float)w, (cmd->clip_x + cmd->clip_w) * sx));
        int cy1 = lg__soft_first(lg__min((float)h, (cmd->clip_y + cmd->clip_h) * sy));
        for (k = 0; k + 2 < cmd->index_count; k += 3) {
            const unsigned int *ix = d->indices + cmd->first_index + k;
            const lg_vertex *a = &d->vertices[ix[0]], *b = &d->vertices[ix[1]], *c = &d->vertices[ix[2]];
            float ax = a->x * sx, ay = a->y * sy, bx = b->x * sx, by = b->y * sy, px = c->x * sx, py = c->y * sy, area;
            int x0, y0, x1, y1, flat = cmd->texture || (a->u == b->u && a->u == c->u && a->v == b->v && a->v == c->v);

            /* two triangles that make an upright box of one colour - nearly everything in an
               interface, every letter too - are filled directly, with no edge tests */
            if (k + 5 < cmd->index_count && ix[3] == ix[0] && ix[4] == ix[2]) {
                const lg_vertex *e = &d->vertices[ix[5]];
                if (a->y == b->y && b->x == c->x && c->y == e->y && e->x == a->x && a->x < b->x && a->y < c->y &&
                    memcmp(&a->r, &b->r, 4) == 0 && memcmp(&a->r, &c->r, 4) == 0 && memcmp(&a->r, &e->r, 4) == 0) {
                    float al = a->a / 255.0f, du = (c->u - a->u) * fw / (px - ax), dv = (c->v - a->v) * fh / (py - ay);
                    x0 = lg__soft_first(ax); x1 = lg__soft_first(px); y0 = lg__soft_first(ay); y1 = lg__soft_first(py);
                    if (x0 < cx0) x0 = cx0;
                    if (y0 < cy0) y0 = cy0;
                    if (x1 > cx1) x1 = cx1;
                    if (y1 > cy1) y1 = cy1;
                    for (y = y0; y < y1; y++) {
                        unsigned char *p = rgba + ((size_t)y * (size_t)w + (size_t)x0) * 4;
                        float v = a->v * fh + ((float)y + 0.5f - ay) * dv;
                        for (x = x0; x < x1; x++, p += 4) {
                            float m = flat ? al : al * lg__soft_font(f, a->u * fw + ((float)x + 0.5f - ax) * du, v);
                            if (m >= 1) { p[0] = a->r; p[1] = a->g; p[2] = a->b; p[3] = 255; }
                            else if (m > 0) lg__soft_blend(p, a->r, a->g, a->b, m);
                        }
                    }
                    k += 3;
                    continue;
                }
            }

            area = lg__edge(ax, ay, bx, by, px, py);
            if (area == 0) continue;
            if (area < 0) { const lg_vertex *t = b; float tx = bx, ty = by; b = c; c = t; bx = px; by = py; px = tx; py = ty; area = -area; }
            x0 = (int)lg__max((float)cx0, lg__floor(lg__min(ax, lg__min(bx, px))));
            y0 = (int)lg__max((float)cy0, lg__floor(lg__min(ay, lg__min(by, py))));
            x1 = (int)lg__min((float)cx1, lg__floor(lg__max(ax, lg__max(bx, px))) + 1);
            y1 = (int)lg__min((float)cy1, lg__floor(lg__max(ay, lg__max(by, py))) + 1);
            for (y = y0; y < y1; y++)
                for (x = x0; x < x1; x++) {
                    float qx = (float)x + 0.5f, qy = (float)y + 0.5f;
                    float w0 = lg__edge(bx, by, px, py, qx, qy);
                    float w1 = lg__edge(px, py, ax, ay, qx, qy);
                    float w2 = lg__edge(ax, ay, bx, by, qx, qy);
                    float al;
                    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                    if ((w0 == 0 && !lg__owns(bx, by, px, py)) || (w1 == 0 && !lg__owns(px, py, ax, ay)) ||
                        (w2 == 0 && !lg__owns(ax, ay, bx, by))) continue;
                    w0 /= area; w1 /= area; w2 /= area;
                    al = (w0 * a->a + w1 * b->a + w2 * c->a) / 255.0f;
                    if (!flat) al *= lg__soft_font(f, (w0 * a->u + w1 * b->u + w2 * c->u) * fw, (w0 * a->v + w1 * b->v + w2 * c->v) * fh);
                    if (al <= 0) continue;
                    lg__soft_blend(rgba + ((size_t)y * (size_t)w + (size_t)x) * 4, w0 * a->r + w1 * b->r + w2 * c->r,
                                   w0 * a->g + w1 * b->g + w2 * c->g, w0 * a->b + w1 * b->b + w2 * c->b, al > 1 ? 1 : al);
                }
        }
    }
}
#endif /* LOGUIA_SOFTWARE */

/* ======================================================================================
   LOGUIA_GL: draw with OpenGL 1.1
   ====================================================================================== */
#ifdef LOGUIA_GL
#if defined(LOGUIA_GL_HEADER)
#include LOGUIA_GL_HEADER
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

static GLuint lg__gl_font;
static const unsigned char *lg__gl_pixels;      /* the font picture that is in the texture */
static unsigned int lg__gl_version;

LGDEF void lg_gl_init(void) {
    const lg_font *f = lg_get_font();
    if (!lg__gl_font) glGenTextures(1, &lg__gl_font);
    glBindTexture(GL_TEXTURE_2D, lg__gl_font);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, f->width, f->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, f->pixels);
    lg__gl_pixels = f->pixels;
    lg__gl_version = f->version;
}
LGDEF void lg_gl_shutdown(void) {
    if (lg__gl_font) glDeleteTextures(1, &lg__gl_font);
    lg__gl_font = 0;
}
LGDEF void lg_gl_render(int fb_w, int fb_h) {
    const lg_draw_list *d = lg_get_draw_list();
    float sx, sy;
    int i;
    if (lg__c.w <= 0 || lg__c.h <= 0 || fb_w <= 0 || fb_h <= 0 || !d->cmd_count) return;
    if (lg__gl_pixels != lg__c.font.pixels || lg__gl_version != lg__c.font.version) lg_gl_init();     /* another font, or new characters in it */
    sx = (float)fb_w / lg__c.w; sy = (float)fb_h / lg__c.h;      /* screens where a point is several pixels */
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT | GL_SCISSOR_BIT | GL_VIEWPORT_BIT | GL_TEXTURE_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glViewport(0, 0, fb_w, fb_h);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, lg__c.w, lg__c.h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glEnable(GL_SCISSOR_TEST); glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(lg_vertex), &d->vertices->x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(lg_vertex), &d->vertices->u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(lg_vertex), &d->vertices->r);
    for (i = 0; i < d->cmd_count; i++) {
        const lg_cmd *c = &d->cmds[i];
        glScissor((GLint)(c->clip_x * sx), (GLint)((float)fb_h - (c->clip_y + c->clip_h) * sy), (GLsizei)(c->clip_w * sx + 0.5f), (GLsizei)(c->clip_h * sy + 0.5f));
        glBindTexture(GL_TEXTURE_2D, c->texture ? (GLuint)(size_t)c->texture : lg__gl_font);
        glDrawElements(GL_TRIANGLES, (GLsizei)c->index_count, GL_UNSIGNED_INT, d->indices + c->first_index);
    }
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glPopClientAttrib();
    glPopAttrib();
}
#endif /* LOGUIA_GL */

/* ======================================================================================
   LOGUIA_GLFW: input from a GLFW window
   ====================================================================================== */
#ifdef LOGUIA_GLFW
#ifndef GLFW_VERSION_MAJOR
#include <GLFW/glfw3.h>
#endif

static GLFWwindow *lg__glfw_win;
static double lg__glfw_time;
static float lg__glfw_mouse = 1;        /* GLFW's mouse coordinates to interface units */

static void lg__glfw_button(GLFWwindow *w, int button, int action, int mods) {
    (void)w; (void)mods;
    lg_input_mouse_button(button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 1 : 2, action != GLFW_RELEASE);
}
static void lg__glfw_cursor(GLFWwindow *w, double x, double y) { (void)w; lg_input_mouse_pos((float)x * lg__glfw_mouse, (float)y * lg__glfw_mouse); }
static void lg__glfw_scroll(GLFWwindow *w, double dx, double dy) { (void)w; lg_input_scroll((float)dx, (float)dy); }
static void lg__glfw_char(GLFWwindow *w, unsigned int cp) { (void)w; lg_input_char(cp); }
static void lg__glfw_key(GLFWwindow *w, int key, int scancode, int action, int mods) {
    int k = LG_KEY_NONE;
    (void)w; (void)scancode; (void)mods;
    switch (key) {
    case GLFW_KEY_LEFT: k = LG_KEY_LEFT; break;         case GLFW_KEY_RIGHT: k = LG_KEY_RIGHT; break;
    case GLFW_KEY_UP: k = LG_KEY_UP; break;             case GLFW_KEY_DOWN: k = LG_KEY_DOWN; break;
    case GLFW_KEY_HOME: k = LG_KEY_HOME; break;         case GLFW_KEY_END: k = LG_KEY_END; break;
    case GLFW_KEY_BACKSPACE: k = LG_KEY_BACKSPACE; break; case GLFW_KEY_DELETE: k = LG_KEY_DELETE; break;
    case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: k = LG_KEY_ENTER; break;
    case GLFW_KEY_TAB: k = LG_KEY_TAB; break;           case GLFW_KEY_ESCAPE: k = LG_KEY_ESCAPE; break;
    case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: k = LG_KEY_SHIFT; break;
    /* on macOS the Command key does what Ctrl does elsewhere */
    case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL: case GLFW_KEY_LEFT_SUPER: case GLFW_KEY_RIGHT_SUPER: k = LG_KEY_CTRL; break;
    case GLFW_KEY_A: k = LG_KEY_A; break;               case GLFW_KEY_C: k = LG_KEY_C; break;
    case GLFW_KEY_V: k = LG_KEY_V; break;               case GLFW_KEY_X: k = LG_KEY_X; break;
    case GLFW_KEY_Z: k = LG_KEY_Z; break;               case GLFW_KEY_Y: k = LG_KEY_Y; break;
    case GLFW_KEY_SPACE: k = LG_KEY_SPACE; break;
    case GLFW_KEY_PAGE_UP: k = LG_KEY_PAGE_UP; break;   case GLFW_KEY_PAGE_DOWN: k = LG_KEY_PAGE_DOWN; break;
    }
    lg_input_key(k, action != GLFW_RELEASE);
}
static const char *lg__glfw_get_clip(void *user) { return glfwGetClipboardString((GLFWwindow *)user); }
static void lg__glfw_set_clip(void *user, const char *text) { glfwSetClipboardString((GLFWwindow *)user, text); }

LGDEF void lg_glfw_init(struct GLFWwindow *window) {
    lg__glfw_win = window;
    lg__glfw_time = glfwGetTime();
    glfwSetMouseButtonCallback(window, lg__glfw_button);
    glfwSetCursorPosCallback(window, lg__glfw_cursor);
    glfwSetScrollCallback(window, lg__glfw_scroll);
    glfwSetCharCallback(window, lg__glfw_char);
    glfwSetKeyCallback(window, lg__glfw_key);
    lg_set_clipboard(lg__glfw_get_clip, lg__glfw_set_clip, window);
}
LGDEF void lg_glfw_begin_frame(void) {
    int w, h, fw, fh;
    float scale = 1, scale_y = 1;
    double now = glfwGetTime();
    glfwGetWindowSize(lg__glfw_win, &w, &h);
    glfwGetFramebufferSize(lg__glfw_win, &fw, &fh);
    /* The interface is measured in units that the screen's scale turns into pixels. Some systems
       (Wayland, macOS) already count the window and the mouse in such units, others (X11, Windows)
       count in pixels: dividing the pixel size by the scale gives the same result on all of them. */
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
    glfwGetWindowContentScale(lg__glfw_win, &scale, &scale_y);
#else
    if (w > 0) scale = (float)fw / (float)w;
#endif
    (void)scale_y;
    if (scale <= 0) scale = 1;
    lg__glfw_mouse = w > 0 ? (float)fw / (float)w / scale : 1;
    lg_set_pixel_scale(scale);
    lg_begin_frame((float)fw / scale, (float)fh / scale, (float)(now - lg__glfw_time));
    lg__glfw_time = now;
}
#endif /* LOGUIA_GLFW */

/* ======================================================================================
   LOGUIA_WINDOW: a native window, drawn with the software module
   ====================================================================================== */
#ifdef LOGUIA_WINDOW
static struct {
    int open, w, h;             /* w, h: the window in pixels */
    int idle;                   /* frames still to draw before it may sleep */
    float scale;                /* pixels per interface unit */
    unsigned char *px;          /* the picture, w * h * 4 bytes */
    char *clip;                 /* text copied or pasted last */
} lg__w;

static void lg__w_size(int w, int h) {
    lg__w.w = w < 1 ? 1 : w; lg__w.h = h < 1 ? 1 : h;
    lg__w.px = (unsigned char *)LOGUIA_REALLOC(lg__w.px, (size_t)lg__w.w * (size_t)lg__w.h * 4);
    lg__w.idle = 3;
}
/* ends the frame and draws it; bgr: the system wants blue first */
static void lg__w_draw(unsigned int bg, int bgr) {
    unsigned char *p = lg__w.px, *end = p + (size_t)lg__w.w * (size_t)lg__w.h * 4, t;
    lg_end_frame();
    for (; p < end; p += 4) { p[0] = (unsigned char)(bg >> 24); p[1] = (unsigned char)(bg >> 16); p[2] = (unsigned char)(bg >> 8); p[3] = 255; }
    lg_soft_render(lg__w.px, lg__w.w, lg__w.h);
    if (bgr) for (p = lg__w.px; p < end; p += 4) { t = p[0]; p[0] = p[2]; p[2] = t; }
}
static void lg__w_begin(float dt) {
    if (lg__w.idle > 0) lg__w.idle--;
    lg_set_pixel_scale(lg__w.scale);
    lg_begin_frame((float)lg__w.w / lg__w.scale, (float)lg__w.h / lg__w.scale, dt);
}

#if defined(_WIN32)
/* ---------------------------------------------------------------- Win32 */
#include <windows.h>
#include <imm.h>

/* the input method functions come from imm32.dll, loaded when the window opens: nothing more to link */
typedef HIMC (WINAPI *lg__imm_get)(HWND);
typedef BOOL (WINAPI *lg__imm_release)(HWND, HIMC);
typedef BOOL (WINAPI *lg__imm_comp)(HIMC, LPCOMPOSITIONFORM);
typedef BOOL (WINAPI *lg__imm_cand)(HIMC, LPCANDIDATEFORM);

static struct {
    HWND hwnd; DWORD last; unsigned int high;
    HMODULE imm; lg__imm_get get; lg__imm_release release; lg__imm_comp comp; lg__imm_cand cand;
    POINT spot;
} lg__w32;

/* the input method's window and its list of candidates go where the caret is */
static void lg__w32_ime(void) {
    float x, y, w, h;
    HIMC ctx;
    COMPOSITIONFORM cf;
    CANDIDATEFORM cand;
    POINT at;
    if (!lg__w32.get || !lg_ime_rect(&x, &y, &w, &h)) return;
    at.x = (LONG)(x * lg__w.scale); at.y = (LONG)(y * lg__w.scale);
    if (at.x == lg__w32.spot.x && at.y == lg__w32.spot.y) return;
    if ((ctx = lg__w32.get(lg__w32.hwnd)) == 0) return;
    lg__w32.spot = at;
    memset(&cf, 0, sizeof cf);
    cf.dwStyle = CFS_POINT; cf.ptCurrentPos = at;
    lg__w32.comp(ctx, &cf);
    memset(&cand, 0, sizeof cand);
    cand.dwStyle = CFS_EXCLUDE; cand.ptCurrentPos = at;
    cand.rcArea.left = at.x; cand.rcArea.top = at.y;
    cand.rcArea.right = at.x + (LONG)(w * lg__w.scale) + 1; cand.rcArea.bottom = at.y + (LONG)(h * lg__w.scale);
    lg__w32.cand(ctx, &cand);
    lg__w32.release(lg__w32.hwnd, ctx);
}

static int lg__w32_key(WPARAM vk) {
    switch (vk) {
    case VK_LEFT: return LG_KEY_LEFT;       case VK_RIGHT: return LG_KEY_RIGHT;
    case VK_UP: return LG_KEY_UP;           case VK_DOWN: return LG_KEY_DOWN;
    case VK_HOME: return LG_KEY_HOME;       case VK_END: return LG_KEY_END;
    case VK_BACK: return LG_KEY_BACKSPACE;  case VK_DELETE: return LG_KEY_DELETE;
    case VK_RETURN: return LG_KEY_ENTER;    case VK_TAB: return LG_KEY_TAB;
    case VK_ESCAPE: return LG_KEY_ESCAPE;   case VK_SPACE: return LG_KEY_SPACE;
    case VK_PRIOR: return LG_KEY_PAGE_UP;   case VK_NEXT: return LG_KEY_PAGE_DOWN;
    case VK_CONTROL: return LG_KEY_CTRL;    case VK_SHIFT: return LG_KEY_SHIFT;
    case 'A': return LG_KEY_A;              case 'C': return LG_KEY_C;
    case 'V': return LG_KEY_V;              case 'X': return LG_KEY_X;
    case 'Z': return LG_KEY_Z;              case 'Y': return LG_KEY_Y;
    }
    return LG_KEY_NONE;
}
static void lg__w32_button(HWND hwnd, int button, int down) {
    if (down) SetCapture(hwnd); else ReleaseCapture();       /* a drag goes on outside the window */
    lg_input_mouse_button(button, down);
}
static LRESULT CALLBACK lg__w32_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    lg__w.idle = 3;
    switch (msg) {
    case WM_CLOSE: lg__w.open = 0; return 0;
    case WM_SIZE: if (wp != SIZE_MINIMIZED) lg__w_size(LOWORD(lp), HIWORD(lp)); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: ValidateRect(hwnd, 0); return 0;
    case WM_MOUSEMOVE: lg_input_mouse_pos((float)(short)LOWORD(lp) / lg__w.scale, (float)(short)HIWORD(lp) / lg__w.scale); return 0;
    case WM_LBUTTONDOWN: lg__w32_button(hwnd, 0, 1); return 0;
    case WM_LBUTTONUP: lg__w32_button(hwnd, 0, 0); return 0;
    case WM_RBUTTONDOWN: lg__w32_button(hwnd, 1, 1); return 0;
    case WM_RBUTTONUP: lg__w32_button(hwnd, 1, 0); return 0;
    case WM_MBUTTONDOWN: lg__w32_button(hwnd, 2, 1); return 0;
    case WM_MBUTTONUP: lg__w32_button(hwnd, 2, 0); return 0;
    case WM_MOUSEWHEEL: lg_input_scroll(0, (float)(short)HIWORD(wp) / 120.0f); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: lg_input_key(lg__w32_key(wp), 1); break;
    case WM_KEYUP: case WM_SYSKEYUP: lg_input_key(lg__w32_key(wp), 0); break;
    case WM_KILLFOCUS: lg_input_key(LG_KEY_CTRL, 0); lg_input_key(LG_KEY_SHIFT, 0); break;
    case WM_CHAR:           /* UTF-16: characters outside the basic plane come in two halves */
        if (wp >= 0xD800 && wp < 0xDC00) lg__w32.high = (unsigned int)wp;
        else if (wp >= 0xDC00 && wp < 0xE000) lg_input_char(0x10000 + ((lg__w32.high - 0xD800) << 10) + ((unsigned int)wp - 0xDC00));
        else lg_input_char((unsigned int)wp);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
static const char *lg__w32_get_clip(void *user) {
    HANDLE h;
    (void)user;
    if (!OpenClipboard(lg__w32.hwnd)) return "";
    h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t *wide = (const wchar_t *)GlobalLock(h);
        if (wide) {
            int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, 0, 0, 0, 0);
            LOGUIA_FREE(lg__w.clip);
            lg__w.clip = (char *)LOGUIA_MALLOC((size_t)n + 1);
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, lg__w.clip, n, 0, 0);
            lg__w.clip[n] = 0;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return h && lg__w.clip ? lg__w.clip : "";
}
static void lg__w32_set_clip(void *user, const char *text) {
    int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, 0, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)n * sizeof(wchar_t));
    (void)user;
    if (!h) return;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, (wchar_t *)GlobalLock(h), n);
    GlobalUnlock(h);
    if (OpenClipboard(lg__w32.hwnd)) {
        EmptyClipboard();
        if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
        CloseClipboard();
    } else GlobalFree(h);
}
LGDEF int lg_window_open(const char *title, int width, int height) {
    WNDCLASSW wc;
    RECT r;
    wchar_t wide[256];
    HDC dc = GetDC(0);
    memset(&lg__w, 0, sizeof lg__w);
    memset(&lg__w32, 0, sizeof lg__w32);
    lg__w.scale = (float)GetDeviceCaps(dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(0, dc);
    if (lg__w.scale < 1) lg__w.scale = 1;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = lg__w32_proc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.lpszClassName = L"Loguia";
    RegisterClassW(&wc);
    r.left = r.top = 0; r.right = (LONG)((float)width * lg__w.scale); r.bottom = (LONG)((float)height * lg__w.scale);
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    wide[MultiByteToWideChar(CP_UTF8, 0, title, -1, wide, 255) > 0 ? 255 : 0] = 0;
    lg_init();
    lg__w.open = 1;
    lg__w32.hwnd = CreateWindowExW(0, L"Loguia", wide, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                   r.right - r.left, r.bottom - r.top, 0, 0, wc.hInstance, 0);
    if (!lg__w32.hwnd) { lg__w.open = 0; lg_shutdown(); return 0; }
    lg_set_clipboard(lg__w32_get_clip, lg__w32_set_clip, 0);
    lg__w32.imm = LoadLibraryA("imm32.dll");
    if (lg__w32.imm) {
        lg__w32.get = (lg__imm_get)(void (*)(void))GetProcAddress(lg__w32.imm, "ImmGetContext");
        lg__w32.release = (lg__imm_release)(void (*)(void))GetProcAddress(lg__w32.imm, "ImmReleaseContext");
        lg__w32.comp = (lg__imm_comp)(void (*)(void))GetProcAddress(lg__w32.imm, "ImmSetCompositionWindow");
        lg__w32.cand = (lg__imm_cand)(void (*)(void))GetProcAddress(lg__w32.imm, "ImmSetCandidateWindow");
        if (!lg__w32.release || !lg__w32.comp || !lg__w32.cand) lg__w32.get = 0;
    }
    lg__w32.spot.x = lg__w32.spot.y = -1;
    ShowWindow(lg__w32.hwnd, SW_SHOW);
    lg__w32.last = GetTickCount();
    return 1;
}
LGDEF int lg_window_begin_frame(void) {
    MSG m;
    DWORD now;
    for (;;) {
        while (PeekMessageW(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        if (!lg__w.open) return 0;
        if (lg__w.idle > 0) break;
        if (MsgWaitForMultipleObjects(0, 0, FALSE, 250, QS_ALLINPUT) == WAIT_TIMEOUT) lg__w.idle = 1;     /* the caret blinks */
    }
    now = GetTickCount();
    lg__w_begin((float)(now - lg__w32.last) / 1000.0f);
    lg__w32.last = now;
    return 1;
}
LGDEF void lg_window_end_frame(unsigned int background_rgba) {
    BITMAPINFO bi;
    HDC dc = GetDC(lg__w32.hwnd);
    lg__w_draw(background_rgba, 1);
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = lg__w.w; bi.bmiHeader.biHeight = -lg__w.h;      /* minus: the first row is the top one */
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, 0, 0, (DWORD)lg__w.w, (DWORD)lg__w.h, 0, 0, 0, (UINT)lg__w.h, lg__w.px, &bi, DIB_RGB_COLORS);
    ReleaseDC(lg__w32.hwnd, dc);
    lg__w32_ime();
}
LGDEF void lg_window_close(void) {
    if (lg__w32.hwnd) DestroyWindow(lg__w32.hwnd);
    if (lg__w32.imm) FreeLibrary(lg__w32.imm);
    LOGUIA_FREE(lg__w.px); LOGUIA_FREE(lg__w.clip);
    memset(&lg__w, 0, sizeof lg__w);
    memset(&lg__w32, 0, sizeof lg__w32);
    lg_shutdown();
}

#elif defined(__APPLE__)
/* ---------------------------------------------------------------- macOS (Cocoa, called through the Objective-C runtime, so this stays C) */
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreGraphics/CoreGraphics.h>

extern id NSDefaultRunLoopMode, NSPasteboardTypeString;

static struct { id app, window, view, pool; double last; } lg__mac;

/* [object message] with nothing, one object or one number as the argument */
static id lg__m(id o, const char *sel) { return ((id (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel)); }
static id lg__m1(id o, const char *sel, id a) { return ((id (*)(id, SEL, id))objc_msgSend)(o, sel_registerName(sel), a); }
static void lg__mi(id o, const char *sel, long a) { ((void (*)(id, SEL, long))objc_msgSend)(o, sel_registerName(sel), a); }
static id lg__mac_class(const char *name) { return (id)objc_getClass(name); }
static id lg__mac_string(const char *text) {
    return ((id (*)(id, SEL, const char *))objc_msgSend)(lg__mac_class("NSString"), sel_registerName("stringWithUTF8String:"), text);
}
static CGRect lg__mac_bounds(void) {
#if defined(__x86_64__)
    return ((CGRect (*)(id, SEL))objc_msgSend_stret)(lg__mac.view, sel_registerName("bounds"));
#else
    return ((CGRect (*)(id, SEL))objc_msgSend)(lg__mac.view, sel_registerName("bounds"));
#endif
}
static int lg__mac_key(unsigned int code) {
    switch (code) {
    case 123: return LG_KEY_LEFT;       case 124: return LG_KEY_RIGHT;
    case 126: return LG_KEY_UP;         case 125: return LG_KEY_DOWN;
    case 115: return LG_KEY_HOME;       case 119: return LG_KEY_END;
    case 51: return LG_KEY_BACKSPACE;   case 117: return LG_KEY_DELETE;
    case 36: case 76: return LG_KEY_ENTER;
    case 48: return LG_KEY_TAB;         case 53: return LG_KEY_ESCAPE;
    case 49: return LG_KEY_SPACE;
    case 116: return LG_KEY_PAGE_UP;    case 121: return LG_KEY_PAGE_DOWN;
    case 0: return LG_KEY_A;            case 8: return LG_KEY_C;
    case 9: return LG_KEY_V;            case 7: return LG_KEY_X;
    case 6: return LG_KEY_Z;            case 16: return LG_KEY_Y;
    }
    return LG_KEY_NONE;
}
/* the view draws the picture that the software module made */
static void lg__mac_draw(id self, SEL cmd, CGRect dirty) {
    CGRect b = lg__mac_bounds();
    CGContextRef ctx = (CGContextRef)lg__m(lg__m(lg__mac_class("NSGraphicsContext"), "currentContext"), "CGContext");
    CGColorSpaceRef space;
    CGDataProviderRef data;
    CGImageRef image;
    (void)self; (void)cmd; (void)dirty;
    if (!ctx || !lg__w.px) return;
    space = CGColorSpaceCreateDeviceRGB();
    data = CGDataProviderCreateWithData(0, lg__w.px, (size_t)lg__w.w * (size_t)lg__w.h * 4, 0);
    image = CGImageCreate((size_t)lg__w.w, (size_t)lg__w.h, 8, 32, (size_t)lg__w.w * 4, space, (CGBitmapInfo)kCGImageAlphaNoneSkipLast, data, 0, 0, kCGRenderingIntentDefault);
    CGContextTranslateCTM(ctx, 0, b.size.height);       /* the view counts from the top, pictures from the bottom */
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, CGRectMake(0, 0, b.size.width, b.size.height), image);
    CGImageRelease(image);
    CGDataProviderRelease(data);
    CGColorSpaceRelease(space);
}
static BOOL lg__mac_yes(id self, SEL cmd) { (void)self; (void)cmd; return YES; }

/* Input methods: the view takes part in the system's text input, so the composing word comes here */
typedef struct { unsigned long location, length; } lg__mac_range;      /* NSRange */
static const char *lg__mac_text(id s) {             /* an NSString or an NSAttributedString, as UTF-8 */
    const char *t;
    if (!s) return "";
    if (((BOOL (*)(id, SEL, id))objc_msgSend)(s, sel_registerName("isKindOfClass:"), lg__mac_class("NSAttributedString"))) s = lg__m(s, "string");
    t = s ? ((const char *(*)(id, SEL))objc_msgSend)(s, sel_registerName("UTF8String")) : 0;
    return t ? t : "";
}
static int lg__mac_byte(const char *t, unsigned long u16) {   /* a place counted in UTF-16 units, as a byte offset */
    const char *p = t, *e = t + strlen(t);
    unsigned long n = 0;
    while (p < e && n < u16) { unsigned int cp; p = lg__utf8(p, e, &cp); n += cp >= 0x10000 ? 2 : 1; }
    return (int)(p - t);
}
static void lg__mac_insert(id self, SEL cmd, id text, lg__mac_range r) {
    const char *t = lg__mac_text(text), *e = t + strlen(t);
    (void)self; (void)cmd; (void)r;
    while (t < e) { unsigned int cp; t = lg__utf8(t, e, &cp); if (cp < 0xF700 || cp > 0xF8FF) lg_input_char(cp); }
    lg_input_compose(0, 0);
    lg__w.idle = 3;
}
static void lg__mac_set_marked(id self, SEL cmd, id text, lg__mac_range sel, lg__mac_range rep) {
    const char *t = lg__mac_text(text);
    (void)self; (void)cmd; (void)rep;
    lg_input_compose(t, lg__mac_byte(t, sel.location));
    lg__w.idle = 3;
}
static void lg__mac_unmark(id self, SEL cmd) { (void)self; (void)cmd; lg_input_compose(0, 0); lg__w.idle = 3; }
static BOOL lg__mac_has_marked(id self, SEL cmd) { (void)self; (void)cmd; return lg__c.compose[0] ? YES : NO; }
static lg__mac_range lg__mac_marked_range(id self, SEL cmd) {
    lg__mac_range r;
    const char *p = lg__c.compose, *e = p + strlen(p);
    (void)self; (void)cmd;
    r.location = lg__c.compose[0] ? 0 : ~0ul >> 1; r.length = 0;        /* nothing marked: NSNotFound */
    while (p < e) { unsigned int cp; p = lg__utf8(p, e, &cp); r.length += cp >= 0x10000 ? 2 : 1; }
    return r;
}
static lg__mac_range lg__mac_selected_range(id self, SEL cmd) { lg__mac_range r; (void)self; (void)cmd; r.location = r.length = 0; return r; }
static id lg__mac_no_attrs(id self, SEL cmd) { (void)self; (void)cmd; return lg__m(lg__mac_class("NSArray"), "array"); }
static id lg__mac_substring(id self, SEL cmd, lg__mac_range r, lg__mac_range *actual) { (void)self; (void)cmd; (void)r; (void)actual; return 0; }
static unsigned long lg__mac_char_at(id self, SEL cmd, CGPoint p) { (void)self; (void)cmd; (void)p; return 0; }
static void lg__mac_command(id self, SEL cmd, SEL what) { (void)self; (void)cmd; (void)what; }   /* keys reach Loguia on their own */
static CGRect lg__mac_rect_call(id o, const char *sel, CGRect r, id arg, int with_arg) {
#if defined(__x86_64__)
    if (with_arg) return ((CGRect (*)(id, SEL, CGRect, id))objc_msgSend_stret)(o, sel_registerName(sel), r, arg);
    return ((CGRect (*)(id, SEL, CGRect))objc_msgSend_stret)(o, sel_registerName(sel), r);
#else
    if (with_arg) return ((CGRect (*)(id, SEL, CGRect, id))objc_msgSend)(o, sel_registerName(sel), r, arg);
    return ((CGRect (*)(id, SEL, CGRect))objc_msgSend)(o, sel_registerName(sel), r);
#endif
}
/* where on the screen the caret is: the list of candidates opens there */
static CGRect lg__mac_first_rect(id self, SEL cmd, lg__mac_range r, lg__mac_range *actual) {
    float x = 0, y = 0, w = 0, h = 0;
    CGRect v;
    (void)self; (void)cmd; (void)r; (void)actual;
    lg_ime_rect(&x, &y, &w, &h);
    v = lg__mac_rect_call(lg__mac.view, "convertRect:toView:", CGRectMake(x, y, w, h), 0, 1);
    return lg__mac_rect_call(lg__mac.window, "convertRectToScreen:", v, 0, 0);
}
static BOOL lg__mac_should_close(id self, SEL cmd, id sender) { (void)self; (void)cmd; (void)sender; lg__w.open = 0; return NO; }

static const char *lg__mac_get_clip(void *user) {
    id text = lg__m1(lg__m(lg__mac_class("NSPasteboard"), "generalPasteboard"), "stringForType:", NSPasteboardTypeString);
    const char *utf8 = text ? ((const char *(*)(id, SEL))objc_msgSend)(text, sel_registerName("UTF8String")) : 0;
    (void)user;
    if (!utf8) return "";
    LOGUIA_FREE(lg__w.clip);            /* the system's own copy lives only until the end of the frame */
    lg__w.clip = (char *)LOGUIA_MALLOC(strlen(utf8) + 1);
    strcpy(lg__w.clip, utf8);
    return lg__w.clip;
}
static void lg__mac_set_clip(void *user, const char *text) {
    id board = lg__m(lg__mac_class("NSPasteboard"), "generalPasteboard");
    (void)user;
    lg__m(board, "clearContents");
    ((BOOL (*)(id, SEL, id, id))objc_msgSend)(board, sel_registerName("setString:forType:"), lg__mac_string(text), NSPasteboardTypeString);
}
/* the size of the view in points, and how many pixels a point is */
static void lg__mac_measure(void) {
    CGRect b = lg__mac_bounds();
    float scale = (float)((CGFloat (*)(id, SEL))objc_msgSend)(lg__mac.window, sel_registerName("backingScaleFactor"));
    int w, h;
    if (scale < 1) scale = 1;
    w = (int)((float)b.size.width * scale + 0.5f); h = (int)((float)b.size.height * scale + 0.5f);
    lg__w.scale = scale;
    if (w != lg__w.w || h != lg__w.h || !lg__w.px) lg__w_size(w, h);
}
static void lg__mac_event(id ev) {
    unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(ev, sel_registerName("type"));
    unsigned long mods = ((unsigned long (*)(id, SEL))objc_msgSend)(ev, sel_registerName("modifierFlags"));
    int key = type == 10 || type == 11, mouse = (type >= 1 && type <= 7) || (type >= 25 && type <= 27);
    lg__w.idle = 3;
    if (mouse) {
        CGPoint at = ((CGPoint (*)(id, SEL))objc_msgSend)(ev, sel_registerName("locationInWindow"));
        float y = (float)(lg__mac_bounds().size.height - at.y);
        lg_input_mouse_pos((float)at.x, y);
        if (y >= 0 || type == 2 || type == 4 || type == 26) {       /* a press on the title bar is not ours, a release always is */
            if (type == 1 || type == 2) lg_input_mouse_button(0, type == 1);
            if (type == 3 || type == 4) lg_input_mouse_button(1, type == 3);
            if (type == 25 || type == 26) lg_input_mouse_button(2, type == 25);
        }
    } else if (type == 22) {
        lg_input_scroll(0, (float)((CGFloat (*)(id, SEL))objc_msgSend)(ev, sel_registerName("scrollingDeltaY")) *
                           (((BOOL (*)(id, SEL))objc_msgSend)(ev, sel_registerName("hasPreciseScrollingDeltas")) ? 0.1f : 1.0f));
    } else if (key || type == 12) {
        /* the Command key does what Ctrl does elsewhere */
        lg_input_key(LG_KEY_CTRL, (mods & ((1ul << 18) | (1ul << 20))) != 0);
        lg_input_key(LG_KEY_SHIFT, (mods & (1ul << 17)) != 0);
        if (key) {
            unsigned int code = ((unsigned short (*)(id, SEL))objc_msgSend)(ev, sel_registerName("keyCode"));
            id ctx = lg__m(lg__mac.view, "inputContext");
            if (type != 10 || !lg__c.compose[0]) lg_input_key(lg__mac_key(code), type == 10);     /* while composing, keys belong to the input method */
            if (type == 10 && (mods & (1ul << 20)) && code == 12) lg__w.open = 0;      /* Command+Q */
            if (type == 10 && !(mods & ((1ul << 18) | (1ul << 20))) && ctx) {
                ((BOOL (*)(id, SEL, id))objc_msgSend)(ctx, sel_registerName("handleEvent:"), ev);     /* text arrives in insertText */
            } else if (type == 10 && !(mods & ((1ul << 18) | (1ul << 20)))) {
                id chars = lg__m(ev, "characters");
                const char *p = chars ? ((const char *(*)(id, SEL))objc_msgSend)(chars, sel_registerName("UTF8String")) : 0, *end;
                for (end = p ? p + strlen(p) : p; p && p < end;) {
                    unsigned int cp;
                    p = lg__utf8(p, end, &cp);
                    if (cp < 0xF700 || cp > 0xF8FF) lg_input_char(cp);     /* arrows and function keys arrive as private characters */
                }
            }
            return;                     /* not passed on: the system would beep at every key */
        }
    }
    lg__m1(lg__mac.app, "sendEvent:", ev);
}
LGDEF int lg_window_open(const char *title, int width, int height) {
    Class view_class, delegate_class;
    id delegate;
    memset(&lg__w, 0, sizeof lg__w);
    memset(&lg__mac, 0, sizeof lg__mac);
    lg__mac.pool = lg__m(lg__m(lg__mac_class("NSAutoreleasePool"), "alloc"), "init");
    lg__mac.app = lg__m(lg__mac_class("NSApplication"), "sharedApplication");
    if (!lg__mac.app) return 0;
    lg__mi(lg__mac.app, "setActivationPolicy:", 0);         /* an ordinary program with a Dock icon */
    view_class = objc_getClass("LoguiaView");
    if (!view_class) {
        view_class = objc_allocateClassPair(objc_getClass("NSView"), "LoguiaView", 0);
        class_addMethod(view_class, sel_registerName("drawRect:"), (IMP)lg__mac_draw, "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
        class_addMethod(view_class, sel_registerName("isFlipped"), (IMP)lg__mac_yes, "c@:");
        class_addMethod(view_class, sel_registerName("acceptsFirstResponder"), (IMP)lg__mac_yes, "c@:");
        class_addMethod(view_class, sel_registerName("insertText:replacementRange:"), (IMP)lg__mac_insert, "v@:@{_NSRange=QQ}");
        class_addMethod(view_class, sel_registerName("setMarkedText:selectedRange:replacementRange:"), (IMP)lg__mac_set_marked, "v@:@{_NSRange=QQ}{_NSRange=QQ}");
        class_addMethod(view_class, sel_registerName("unmarkText"), (IMP)lg__mac_unmark, "v@:");
        class_addMethod(view_class, sel_registerName("hasMarkedText"), (IMP)lg__mac_has_marked, "c@:");
        class_addMethod(view_class, sel_registerName("markedRange"), (IMP)lg__mac_marked_range, "{_NSRange=QQ}@:");
        class_addMethod(view_class, sel_registerName("selectedRange"), (IMP)lg__mac_selected_range, "{_NSRange=QQ}@:");
        class_addMethod(view_class, sel_registerName("validAttributesForMarkedText"), (IMP)lg__mac_no_attrs, "@@:");
        class_addMethod(view_class, sel_registerName("attributedSubstringForProposedRange:actualRange:"), (IMP)lg__mac_substring, "@@:{_NSRange=QQ}^{_NSRange=QQ}");
        class_addMethod(view_class, sel_registerName("characterIndexForPoint:"), (IMP)lg__mac_char_at, "Q@:{CGPoint=dd}");
        class_addMethod(view_class, sel_registerName("firstRectForCharacterRange:actualRange:"), (IMP)lg__mac_first_rect, "{CGRect={CGPoint=dd}{CGSize=dd}}@:{_NSRange=QQ}^{_NSRange=QQ}");
        class_addMethod(view_class, sel_registerName("doCommandBySelector:"), (IMP)lg__mac_command, "v@::");
        {
            Protocol *text_input = objc_getProtocol("NSTextInputClient");
            if (text_input) class_addProtocol(view_class, text_input);
        }
        objc_registerClassPair(view_class);
        delegate_class = objc_allocateClassPair(objc_getClass("NSObject"), "LoguiaDelegate", 0);
        class_addMethod(delegate_class, sel_registerName("windowShouldClose:"), (IMP)lg__mac_should_close, "c@:@");
        objc_registerClassPair(delegate_class);
    }
    lg__mac.window = ((id (*)(id, SEL, CGRect, unsigned long, unsigned long, BOOL))objc_msgSend)(
        lg__m(lg__mac_class("NSWindow"), "alloc"), sel_registerName("initWithContentRect:styleMask:backing:defer:"),
        CGRectMake(0, 0, width, height), 15ul /* title, close, minimise, resize */, 2ul, NO);
    if (!lg__mac.window) return 0;
    lg__mac.view = lg__m(lg__m((id)view_class, "alloc"), "init");
    delegate = lg__m(lg__m(lg__mac_class("LoguiaDelegate"), "alloc"), "init");
    lg__m1(lg__mac.window, "setTitle:", lg__mac_string(title));
    lg__m1(lg__mac.window, "setContentView:", lg__mac.view);
    lg__m1(lg__mac.window, "setDelegate:", delegate);
    lg__mi(lg__mac.window, "setAcceptsMouseMovedEvents:", 1);
    lg__mi(lg__mac.window, "setReleasedWhenClosed:", 0);
    lg__m(lg__mac.window, "center");
    lg__m1(lg__mac.window, "makeFirstResponder:", lg__mac.view);
    lg__m(lg__mac.app, "finishLaunching");
    lg__m1(lg__mac.window, "makeKeyAndOrderFront:", (id)0);
    lg__mi(lg__mac.app, "activateIgnoringOtherApps:", 1);
    lg_init();
    lg_set_clipboard(lg__mac_get_clip, lg__mac_set_clip, 0);
    lg__mac_measure();
    lg__mac.last = CFAbsoluteTimeGetCurrent();
    lg__w.open = 1;
    return 1;
}
LGDEF int lg_window_begin_frame(void) {
    double now;
    if (!lg__w.open) return 0;
    for (;;) {
        id until, ev;
        lg__m(lg__mac.pool, "drain");           /* what the system lent us during the last frame goes back */
        lg__mac.pool = lg__m(lg__m(lg__mac_class("NSAutoreleasePool"), "alloc"), "init");
        until = lg__w.idle > 0 ? lg__m(lg__mac_class("NSDate"), "distantPast")
                               : ((id (*)(id, SEL, double))objc_msgSend)(lg__mac_class("NSDate"), sel_registerName("dateWithTimeIntervalSinceNow:"), 0.25);
        ev = ((id (*)(id, SEL, unsigned long, id, id, BOOL))objc_msgSend)(lg__mac.app, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"),
                                                                       ~0ul, until, NSDefaultRunLoopMode, YES);
        if (ev) {
            lg__mac_event(ev);
            /* take whatever else is waiting, without sleeping */
            while ((ev = ((id (*)(id, SEL, unsigned long, id, id, BOOL))objc_msgSend)(lg__mac.app, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"),
                                                                                   ~0ul, lg__m(lg__mac_class("NSDate"), "distantPast"), NSDefaultRunLoopMode, YES)) != 0)
                lg__mac_event(ev);
        } else if (lg__w.idle <= 0) lg__w.idle = 1;     /* nothing happened: one frame, so the caret blinks */
        if (!lg__w.open) return 0;
        if (lg__w.idle > 0) break;
    }
    lg__mac_measure();
    now = CFAbsoluteTimeGetCurrent();
    lg__w_begin((float)(now - lg__mac.last));
    lg__mac.last = now;
    return 1;
}
LGDEF void lg_window_end_frame(unsigned int background_rgba) {
    lg__w_draw(background_rgba, 0);
    lg__m(lg__mac.view, "display");
}
LGDEF void lg_window_close(void) {
    if (lg__mac.window) lg__m(lg__mac.window, "close");
    if (lg__mac.pool) lg__m(lg__mac.pool, "drain");
    LOGUIA_FREE(lg__w.px); LOGUIA_FREE(lg__w.clip);
    memset(&lg__w, 0, sizeof lg__w);
    memset(&lg__mac, 0, sizeof lg__mac);
    lg_shutdown();
}

#else
#include <poll.h>
#include <time.h>

static void lg__w_keep(const char *text, size_t n) {
    LOGUIA_FREE(lg__w.clip);
    lg__w.clip = (char *)LOGUIA_MALLOC(n + 1);
    memcpy(lg__w.clip, text, n);
    lg__w.clip[n] = 0;
}

#ifdef LOGUIA_WAYLAND
/* ---------------------------------------------------------------- Wayland (Linux, BSD) */
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

/* The messages Loguia uses from protocols that libwayland does not carry itself. A tool
   (wayland-scanner) normally writes these tables; the few that are needed are written out here.
   Only the first requests of each list are there: the ones up to the last that is used. */
static const struct wl_interface *lg__wl_no[4];
#define LG__WL_M(name, args) { name, args, lg__wl_no }
static const struct wl_message lg__xdg_wm_base_req[] = { LG__WL_M("destroy", ""), LG__WL_M("create_positioner", "n"), LG__WL_M("get_xdg_surface", "no"), LG__WL_M("pong", "u") };
static const struct wl_message lg__xdg_wm_base_ev[] = { LG__WL_M("ping", "u") };
static const struct wl_interface lg__xdg_wm_base_if = { "xdg_wm_base", 1, 4, lg__xdg_wm_base_req, 1, lg__xdg_wm_base_ev };
static const struct wl_message lg__xdg_surface_req[] = { LG__WL_M("destroy", ""), LG__WL_M("get_toplevel", "n"), LG__WL_M("get_popup", "n?oo"), LG__WL_M("set_window_geometry", "iiii"), LG__WL_M("ack_configure", "u") };
static const struct wl_message lg__xdg_surface_ev[] = { LG__WL_M("configure", "u") };
static const struct wl_interface lg__xdg_surface_if = { "xdg_surface", 1, 5, lg__xdg_surface_req, 1, lg__xdg_surface_ev };
static const struct wl_message lg__xdg_toplevel_req[] = { LG__WL_M("destroy", ""), LG__WL_M("set_parent", "?o"), LG__WL_M("set_title", "s"), LG__WL_M("set_app_id", "s") };
static const struct wl_message lg__xdg_toplevel_ev[] = { LG__WL_M("configure", "iia"), LG__WL_M("close", "") };
static const struct wl_interface lg__xdg_toplevel_if = { "xdg_toplevel", 1, 4, lg__xdg_toplevel_req, 2, lg__xdg_toplevel_ev };
static const struct wl_message lg__deco_mgr_req[] = { LG__WL_M("destroy", ""), LG__WL_M("get_toplevel_decoration", "no") };
static const struct wl_interface lg__deco_mgr_if = { "zxdg_decoration_manager_v1", 1, 2, lg__deco_mgr_req, 0, 0 };
static const struct wl_message lg__deco_req[] = { LG__WL_M("destroy", ""), LG__WL_M("set_mode", "u") };
static const struct wl_message lg__deco_ev[] = { LG__WL_M("configure", "u") };
static const struct wl_interface lg__deco_if = { "zxdg_toplevel_decoration_v1", 1, 2, lg__deco_req, 1, lg__deco_ev };
static const struct wl_message lg__viewporter_req[] = { LG__WL_M("destroy", ""), LG__WL_M("get_viewport", "no") };
static const struct wl_interface lg__viewporter_if = { "wp_viewporter", 1, 2, lg__viewporter_req, 0, 0 };
static const struct wl_message lg__viewport_req[] = { LG__WL_M("destroy", ""), LG__WL_M("set_source", "ffff"), LG__WL_M("set_destination", "ii") };
static const struct wl_interface lg__viewport_if = { "wp_viewport", 1, 3, lg__viewport_req, 0, 0 };
static const struct wl_message lg__frac_mgr_req[] = { LG__WL_M("destroy", ""), LG__WL_M("get_fractional_scale", "no") };
static const struct wl_interface lg__frac_mgr_if = { "wp_fractional_scale_manager_v1", 1, 2, lg__frac_mgr_req, 0, 0 };
static const struct wl_message lg__frac_req[] = { LG__WL_M("destroy", "") };
static const struct wl_message lg__frac_ev[] = { LG__WL_M("preferred_scale", "u") };
static const struct wl_interface lg__frac_if = { "wp_fractional_scale_v1", 1, 1, lg__frac_req, 1, lg__frac_ev };
static const struct wl_message lg__shape_mgr_req[] = { LG__WL_M("destroy", ""), LG__WL_M("get_pointer", "no") };
static const struct wl_interface lg__shape_mgr_if = { "wp_cursor_shape_manager_v1", 1, 2, lg__shape_mgr_req, 0, 0 };
static const struct wl_message lg__shape_req[] = { LG__WL_M("destroy", ""), LG__WL_M("set_shape", "uu") };
static const struct wl_interface lg__shape_if = { "wp_cursor_shape_device_v1", 1, 2, lg__shape_req, 0, 0 };

/* input methods: the program tells where the caret is, the compositor tells what is being composed */
static const struct wl_message lg__ti_mgr_req[] = { LG__WL_M("destroy", ""), LG__WL_M("get_text_input", "no") };
static const struct wl_interface lg__ti_mgr_if = { "zwp_text_input_manager_v3", 1, 2, lg__ti_mgr_req, 0, 0 };
static const struct wl_message lg__ti_req[] = { LG__WL_M("destroy", ""), LG__WL_M("enable", ""), LG__WL_M("disable", ""),
    LG__WL_M("set_surrounding_text", "sii"), LG__WL_M("set_text_change_cause", "u"), LG__WL_M("set_content_type", "uu"),
    LG__WL_M("set_cursor_rectangle", "iiii"), LG__WL_M("commit", "") };
static const struct wl_message lg__ti_ev[] = { LG__WL_M("enter", "o"), LG__WL_M("leave", "o"), LG__WL_M("preedit_string", "?sii"),
    LG__WL_M("commit_string", "?s"), LG__WL_M("delete_surrounding_text", "uu"), LG__WL_M("done", "u") };
static const struct wl_interface lg__ti_if = { "zwp_text_input_v3", 1, 8, lg__ti_req, 6, lg__ti_ev };

typedef struct { struct wl_buffer *buf; unsigned char *data; size_t size; int w, h, busy; } lg__wl_buf;

static struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct wl_surface *surface;
    struct wl_data_device_manager *data_mgr;
    struct wl_data_device *data_dev;
    struct wl_data_source *source;          /* set while the clipboard holds our text */
    struct wl_data_offer *offer, *fresh;    /* the clipboard of another program; the offer that was announced last */
    struct wl_proxy *wm_base, *xdg_surface, *toplevel, *deco_mgr, *deco, *viewporter, *viewport, *frac_mgr, *frac, *shape_mgr, *shape;
    struct wl_proxy *ti_mgr, *ti;
    int ti_in, ti_on, ti_caret, ti_rect[4];
    char *ti_pre, *ti_commit;           /* what arrived since the last "done" */
    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    lg__wl_buf bufs[3];
    int lw, lh, want_w, want_h;             /* the window in the compositor's units; the size it asked for */
    int configured, out_scale, comp_version, offer_text, fresh_text, rate, delay;
    unsigned int serial, repeat_key;
    double repeat_at, last;
    struct wl_registry_listener registry_l;
    struct wl_seat_listener seat_l;
    struct wl_pointer_listener pointer_l;
    struct wl_keyboard_listener keyboard_l;
    struct wl_output_listener output_l;
    struct wl_buffer_listener buffer_l;
    struct wl_data_device_listener device_l;
    struct wl_data_offer_listener offer_l;
    struct wl_data_source_listener source_l;
} lg__wl;

static double lg__wl_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static unsigned int lg__wl_ver(struct wl_proxy *p) { return wl_proxy_get_version(p); }
static void lg__wl_resize(void) {
    int w = (int)((float)lg__wl.lw * lg__w.scale + 0.5f), h = (int)((float)lg__wl.lh * lg__w.scale + 0.5f);
    if (w != lg__w.w || h != lg__w.h || !lg__w.px) lg__w_size(w, h);
    lg__w.idle = 3;
}

/* --- input methods */
static char *lg__wl_copy(const char *t) {
    size_t n = t ? strlen(t) : 0;
    char *c = (char *)LOGUIA_MALLOC(n + 1);
    if (n) memcpy(c, t, n);
    c[n] = 0;
    return c;
}
static void lg__wl_ti_enter(void *d, struct wl_proxy *t, struct wl_surface *s) { (void)d; (void)t; (void)s; lg__wl.ti_in = 1; lg__wl.ti_on = 0; lg__w.idle = 3; }
static void lg__wl_ti_leave(void *d, struct wl_proxy *t, struct wl_surface *s) {
    (void)d; (void)s;
    if (lg__wl.ti_on) { wl_proxy_marshal_flags(t, 2, 0, 1, 0); wl_proxy_marshal_flags(t, 7, 0, 1, 0); }       /* disable, commit */
    lg__wl.ti_in = lg__wl.ti_on = 0;
    lg_input_compose(0, 0);
    lg__w.idle = 3;
}
static void lg__wl_ti_preedit(void *d, struct wl_proxy *t, const char *text, int32_t begin, int32_t end) {
    (void)d; (void)t; (void)end;
    LOGUIA_FREE(lg__wl.ti_pre);
    lg__wl.ti_pre = text ? lg__wl_copy(text) : 0;
    lg__wl.ti_caret = begin;
}
static void lg__wl_ti_commit(void *d, struct wl_proxy *t, const char *text) {
    (void)d; (void)t;
    LOGUIA_FREE(lg__wl.ti_commit);
    lg__wl.ti_commit = text ? lg__wl_copy(text) : 0;
}
static void lg__wl_ti_delete(void *d, struct wl_proxy *t, uint32_t before, uint32_t after) { (void)d; (void)t; (void)before; (void)after; }
static void lg__wl_ti_done(void *d, struct wl_proxy *t, uint32_t serial) {
    (void)d; (void)t; (void)serial;
    if (lg__wl.ti_commit) {             /* the finished text, typed as if from the keyboard */
        const char *p = lg__wl.ti_commit, *e = p + strlen(p);
        while (p < e) { unsigned int cp; p = lg__utf8(p, e, &cp); lg_input_char(cp); }
    }
    lg_input_compose(lg__wl.ti_pre, lg__wl.ti_pre ? lg__wl.ti_caret : 0);
    LOGUIA_FREE(lg__wl.ti_pre); LOGUIA_FREE(lg__wl.ti_commit);
    lg__wl.ti_pre = lg__wl.ti_commit = 0;
    lg__w.idle = 3;
}
static void (*lg__wl_ti_l[])(void) = { (void (*)(void))lg__wl_ti_enter, (void (*)(void))lg__wl_ti_leave, (void (*)(void))lg__wl_ti_preedit,
                                       (void (*)(void))lg__wl_ti_commit, (void (*)(void))lg__wl_ti_delete, (void (*)(void))lg__wl_ti_done };
/* turned on while a text field is being typed into, with the caret's place for the list of candidates */
static void lg__wl_ime(void) {
    float x = 0, y = 0, w = 0, h = 0;
    int on = lg__wl.ti && lg__wl.ti_in && lg_ime_rect(&x, &y, &w, &h), changed = 0, r[4];
    if (!lg__wl.ti) return;
    if (on != lg__wl.ti_on) {
        wl_proxy_marshal_flags(lg__wl.ti, on ? 1 : 2, 0, 1, 0);
        lg__wl.ti_on = on; changed = 1;
        lg__wl.ti_rect[2] = -1;
    }
    r[0] = (int)x; r[1] = (int)y; r[2] = (int)w + 1; r[3] = (int)h;
    if (on && memcmp(r, lg__wl.ti_rect, sizeof r) != 0) {
        wl_proxy_marshal_flags(lg__wl.ti, 6, 0, 1, 0, r[0], r[1], r[2], r[3]);
        memcpy(lg__wl.ti_rect, r, sizeof r);
        changed = 1;
    }
    if (changed) wl_proxy_marshal_flags(lg__wl.ti, 7, 0, 1, 0);
}

/* --- the window */
static void lg__wl_ping(void *d, struct wl_proxy *base, uint32_t serial) { (void)d; wl_proxy_marshal_flags(base, 3, 0, lg__wl_ver(base), 0, serial); }
static void lg__wl_surface_configure(void *d, struct wl_proxy *surface, uint32_t serial) {
    (void)d;
    wl_proxy_marshal_flags(surface, 4, 0, lg__wl_ver(surface), 0, serial);     /* ack_configure */
    if (lg__wl.want_w > 0 && lg__wl.want_h > 0) { lg__wl.lw = lg__wl.want_w; lg__wl.lh = lg__wl.want_h; }
    lg__wl.configured = 1;
    lg__wl_resize();
}
static void lg__wl_toplevel_configure(void *d, struct wl_proxy *top, int32_t w, int32_t h, struct wl_array *states) {
    (void)d; (void)top; (void)states;
    lg__wl.want_w = w; lg__wl.want_h = h;       /* 0 means: whatever size the program likes */
}
static void lg__wl_toplevel_close(void *d, struct wl_proxy *top) { (void)d; (void)top; lg__w.open = 0; lg__w.idle = 3; }
static void lg__wl_deco_configure(void *d, struct wl_proxy *deco, uint32_t mode) { (void)d; (void)deco; (void)mode; }
static void lg__wl_frac_scale(void *d, struct wl_proxy *frac, uint32_t scale) {       /* in 120ths: 150 is 125 % */
    (void)d; (void)frac;
    lg__w.scale = (float)scale / 120.0f;
    if (lg__w.scale < 0.5f) lg__w.scale = 1;
    if (lg__wl.configured) lg__wl_resize();
}
static void (*lg__wl_base_l[])(void) = { (void (*)(void))lg__wl_ping };
static void (*lg__wl_surface_l[])(void) = { (void (*)(void))lg__wl_surface_configure };
static void (*lg__wl_toplevel_l[])(void) = { (void (*)(void))lg__wl_toplevel_configure, (void (*)(void))lg__wl_toplevel_close };
static void (*lg__wl_deco_l[])(void) = { (void (*)(void))lg__wl_deco_configure };
static void (*lg__wl_frac_l[])(void) = { (void (*)(void))lg__wl_frac_scale };

/* --- the picture: shared memory that the compositor reads */
static void lg__wl_buffer_release(void *d, struct wl_buffer *b) { (void)b; ((lg__wl_buf *)d)->busy = 0; }
static void lg__wl_buffer_free(lg__wl_buf *b) {
    if (b->buf) wl_buffer_destroy(b->buf);
    if (b->data) munmap(b->data, b->size);
    memset(b, 0, sizeof *b);
}
static lg__wl_buf *lg__wl_buffer(int w, int h) {
    static unsigned int count;
    int i, fd;
    for (i = 0; i < 3; i++) {
        lg__wl_buf *b = &lg__wl.bufs[i];
        struct wl_shm_pool *pool;
        char name[64];
        if (b->busy) continue;                  /* the compositor is still showing it */
        if (b->buf && b->w == w && b->h == h) return b;
        lg__wl_buffer_free(b);
        b->size = (size_t)w * (size_t)h * 4;
        snprintf(name, sizeof name, "/loguia-%d-%u", (int)getpid(), count++);
        fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd < 0) return 0;
        shm_unlink(name);
        if (ftruncate(fd, (off_t)b->size) != 0) { close(fd); return 0; }
        b->data = (unsigned char *)mmap(0, b->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (b->data == MAP_FAILED) { b->data = 0; close(fd); return 0; }
        pool = wl_shm_create_pool(lg__wl.shm, fd, (int32_t)b->size);
        b->buf = wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);
        wl_buffer_add_listener(b->buf, &lg__wl.buffer_l, b);
        b->w = w; b->h = h;
        return b;
    }
    return 0;
}

/* --- the mouse */
static void lg__wl_pointer_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)s;
    lg__wl.serial = serial;
    lg__w.idle = 3;
    lg_input_mouse_pos((float)wl_fixed_to_double(x), (float)wl_fixed_to_double(y));
    /* a Wayland program has to say which cursor it wants; 1 is the ordinary arrow */
    if (!lg__wl.shape && lg__wl.shape_mgr) lg__wl.shape = wl_proxy_marshal_flags(lg__wl.shape_mgr, 1, &lg__shape_if, lg__wl_ver(lg__wl.shape_mgr), 0, (void *)0, p);
    if (lg__wl.shape) wl_proxy_marshal_flags(lg__wl.shape, 1, 0, lg__wl_ver(lg__wl.shape), 0, serial, 1u);
}
static void lg__wl_pointer_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)p; (void)serial; (void)s;
    lg__w.idle = 3;
    lg_input_mouse_pos(-1000, -1000);
}
static void lg__wl_pointer_motion(void *d, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)p; (void)time;
    lg__w.idle = 3;
    lg_input_mouse_pos((float)wl_fixed_to_double(x), (float)wl_fixed_to_double(y));
}
static void lg__wl_pointer_button(void *d, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)d; (void)p; (void)time;
    lg__wl.serial = serial;
    lg__w.idle = 3;
    if (button >= 0x110 && button <= 0x112) lg_input_mouse_button(button == 0x110 ? 0 : button == 0x111 ? 1 : 2, state != 0);    /* left, right, middle */
}
static void lg__wl_pointer_axis(void *d, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)d; (void)p; (void)time;
    lg__w.idle = 3;
    if (axis == 0) lg_input_scroll(0, (float)wl_fixed_to_double(value) / -15.0f);       /* one notch of a wheel is 15 */
}
static void lg__wl_pointer_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void lg__wl_pointer_source(void *d, struct wl_pointer *p, uint32_t source) { (void)d; (void)p; (void)source; }
static void lg__wl_pointer_stop(void *d, struct wl_pointer *p, uint32_t time, uint32_t axis) { (void)d; (void)p; (void)time; (void)axis; }
static void lg__wl_pointer_discrete(void *d, struct wl_pointer *p, uint32_t axis, int32_t n) { (void)d; (void)p; (void)axis; (void)n; }

/* --- the keyboard */
static int lg__wl_key(xkb_keysym_t k) {
    switch (xkb_keysym_to_lower(k)) {
    case XKB_KEY_Left: return LG_KEY_LEFT;              case XKB_KEY_Right: return LG_KEY_RIGHT;
    case XKB_KEY_Up: return LG_KEY_UP;                  case XKB_KEY_Down: return LG_KEY_DOWN;
    case XKB_KEY_Home: return LG_KEY_HOME;              case XKB_KEY_End: return LG_KEY_END;
    case XKB_KEY_BackSpace: return LG_KEY_BACKSPACE;    case XKB_KEY_Delete: return LG_KEY_DELETE;
    case XKB_KEY_Return: case XKB_KEY_KP_Enter: return LG_KEY_ENTER;
    case XKB_KEY_Tab: case XKB_KEY_ISO_Left_Tab: return LG_KEY_TAB;
    case XKB_KEY_Escape: return LG_KEY_ESCAPE;          case XKB_KEY_space: return LG_KEY_SPACE;
    case XKB_KEY_Prior: return LG_KEY_PAGE_UP;          case XKB_KEY_Next: return LG_KEY_PAGE_DOWN;
    case XKB_KEY_a: return LG_KEY_A;                    case XKB_KEY_c: return LG_KEY_C;
    case XKB_KEY_v: return LG_KEY_V;                    case XKB_KEY_x: return LG_KEY_X;
    case XKB_KEY_z: return LG_KEY_Z;                    case XKB_KEY_y: return LG_KEY_Y;
    }
    return LG_KEY_NONE;
}
/* a key went down, or is held and repeats: the key itself, and the text it types */
static void lg__wl_press(unsigned int code) {
    char text[32];
    const char *p = text, *end;
    int n;
    if (!lg__wl.state) return;
    lg_input_key(lg__wl_key(xkb_state_key_get_one_sym(lg__wl.state, code)), 1);
    n = xkb_state_key_get_utf8(lg__wl.state, code, text, sizeof text);
    if (n < 0 || n >= (int)sizeof text) n = 0;
    for (end = text + n; p < end;) { unsigned int cp; p = lg__utf8(p, end, &cp); lg_input_char(cp); }
}
static void lg__wl_keymap(void *d, struct wl_keyboard *k, uint32_t format, int32_t fd, uint32_t size) {
    char *map;
    (void)d; (void)k;
    if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && (map = (char *)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0)) != MAP_FAILED) {
        if (lg__wl.state) xkb_state_unref(lg__wl.state);
        if (lg__wl.keymap) xkb_keymap_unref(lg__wl.keymap);
        lg__wl.keymap = xkb_keymap_new_from_string(lg__wl.xkb, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        lg__wl.state = lg__wl.keymap ? xkb_state_new(lg__wl.keymap) : 0;
        munmap(map, size);
    }
    close(fd);
}
static void lg__wl_key_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys) {
    (void)d; (void)k; (void)s; (void)keys;
    lg__wl.serial = serial;
    lg__w.idle = 3;
}
static void lg__wl_key_leave(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)k; (void)serial; (void)s;
    lg__wl.repeat_key = 0;
    lg__w.idle = 3;
    lg_input_key(LG_KEY_CTRL, 0); lg_input_key(LG_KEY_SHIFT, 0);
}
static void lg__wl_key_event(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    unsigned int code = key + 8;        /* xkb counts keys from 8 */
    (void)d; (void)k; (void)time;
    lg__wl.serial = serial;
    lg__w.idle = 3;
    if (!lg__wl.state) return;
    if (state) {
        lg__wl_press(code);
        if (lg__wl.rate > 0 && xkb_keymap_key_repeats(lg__wl.keymap, code)) { lg__wl.repeat_key = code; lg__wl.repeat_at = lg__wl_now() + lg__wl.delay / 1000.0; }
    } else {
        lg_input_key(lg__wl_key(xkb_state_key_get_one_sym(lg__wl.state, code)), 0);
        if (lg__wl.repeat_key == code) lg__wl.repeat_key = 0;
    }
}
static void lg__wl_key_mods(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t down, uint32_t latched, uint32_t locked, uint32_t group) {
    (void)d; (void)k; (void)serial;
    if (!lg__wl.state) return;
    xkb_state_update_mask(lg__wl.state, down, latched, locked, 0, 0, group);
    /* Ctrl and Shift are read from here, not from their keys: this is also right for sticky keys and remapped ones */
    lg_input_key(LG_KEY_CTRL, xkb_state_mod_name_is_active(lg__wl.state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0);
    lg_input_key(LG_KEY_SHIFT, xkb_state_mod_name_is_active(lg__wl.state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0);
}
static void lg__wl_key_repeat(void *d, struct wl_keyboard *k, int32_t rate, int32_t delay) { (void)d; (void)k; lg__wl.rate = rate; lg__wl.delay = delay; }

static void lg__wl_seat_caps(void *d, struct wl_seat *seat, uint32_t caps) {
    (void)d;
    /* mice and keyboards come and go (a plug is pulled, the last one is switched off): each time there is a new object */
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !lg__wl.pointer) {
        lg__wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(lg__wl.pointer, &lg__wl.pointer_l, 0);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && lg__wl.pointer) {
        if (lg__wl.shape) wl_proxy_marshal_flags(lg__wl.shape, 0, 0, lg__wl_ver(lg__wl.shape), WL_MARSHAL_FLAG_DESTROY);
        wl_pointer_destroy(lg__wl.pointer);
        lg__wl.pointer = 0; lg__wl.shape = 0;
        lg_input_mouse_button(0, 0); lg_input_mouse_button(1, 0); lg_input_mouse_button(2, 0);
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !lg__wl.keyboard) {
        lg__wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(lg__wl.keyboard, &lg__wl.keyboard_l, 0);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && lg__wl.keyboard) {
        wl_keyboard_destroy(lg__wl.keyboard);
        lg__wl.keyboard = 0; lg__wl.repeat_key = 0;
        lg_input_key(LG_KEY_CTRL, 0); lg_input_key(LG_KEY_SHIFT, 0);
    }
    lg__w.idle = 3;
}
static void lg__wl_seat_name(void *d, struct wl_seat *seat, const char *name) { (void)d; (void)seat; (void)name; }

/* --- screens: only their scale matters, and only when the compositor cannot tell a finer one */
static void lg__wl_out_geometry(void *d, struct wl_output *o, int32_t x, int32_t y, int32_t pw, int32_t ph, int32_t sub, const char *make, const char *model, int32_t turn) {
    (void)d; (void)o; (void)x; (void)y; (void)pw; (void)ph; (void)sub; (void)make; (void)model; (void)turn;
}
static void lg__wl_out_mode(void *d, struct wl_output *o, uint32_t flags, int32_t w, int32_t h, int32_t refresh) { (void)d; (void)o; (void)flags; (void)w; (void)h; (void)refresh; }
static void lg__wl_out_done(void *d, struct wl_output *o) { (void)d; (void)o; }
static void lg__wl_out_scale(void *d, struct wl_output *o, int32_t scale) { (void)d; (void)o; if (scale > lg__wl.out_scale) lg__wl.out_scale = scale; }

/* --- the clipboard */
static void lg__wl_offer_type(void *d, struct wl_data_offer *o, const char *mime) {
    (void)d;
    if (o != lg__wl.fresh) return;
    if (strcmp(mime, "text/plain;charset=utf-8") == 0) lg__wl.fresh_text = 1;
    else if (strcmp(mime, "text/plain") == 0 && !lg__wl.fresh_text) lg__wl.fresh_text = 2;
}
static void lg__wl_device_offer(void *d, struct wl_data_device *dev, struct wl_data_offer *o) {
    (void)d; (void)dev;
    lg__wl.fresh = o; lg__wl.fresh_text = 0;
    wl_data_offer_add_listener(o, &lg__wl.offer_l, 0);
}
static void lg__wl_device_enter(void *d, struct wl_data_device *dev, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *o) {
    (void)d; (void)dev; (void)serial; (void)s; (void)x; (void)y;
    if (o) { if (o == lg__wl.fresh) lg__wl.fresh = 0; wl_data_offer_destroy(o); }       /* something is dragged over the window: not taken */
}
static void lg__wl_device_leave(void *d, struct wl_data_device *dev) { (void)d; (void)dev; }
static void lg__wl_device_motion(void *d, struct wl_data_device *dev, uint32_t time, wl_fixed_t x, wl_fixed_t y) { (void)d; (void)dev; (void)time; (void)x; (void)y; }
static void lg__wl_device_drop(void *d, struct wl_data_device *dev) { (void)d; (void)dev; }
static void lg__wl_device_selection(void *d, struct wl_data_device *dev, struct wl_data_offer *o) {
    (void)d; (void)dev;
    if (lg__wl.offer && lg__wl.offer != o) wl_data_offer_destroy(lg__wl.offer);
    lg__wl.offer = o;
    lg__wl.offer_text = o && o == lg__wl.fresh ? lg__wl.fresh_text : 0;
}
static void lg__wl_source_target(void *d, struct wl_data_source *s, const char *mime) { (void)d; (void)s; (void)mime; }
/* another program pastes what we copied */
static void lg__wl_source_send(void *d, struct wl_data_source *s, const char *mime, int32_t fd) {
    const char *p = lg__w.clip ? lg__w.clip : "";
    size_t left = strlen(p);
    void (*old)(int) = signal(SIGPIPE, SIG_IGN);        /* it may stop reading half-way */
    (void)d; (void)s; (void)mime;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n <= 0) break;
        p += n; left -= (size_t)n;
    }
    close(fd);
    signal(SIGPIPE, old);
}
static void lg__wl_source_cancelled(void *d, struct wl_data_source *s) {      /* somebody else copied something */
    (void)d;
    if (lg__wl.source == s) lg__wl.source = 0;
    wl_data_source_destroy(s);
}
static void lg__wl_set_clip(void *user, const char *text) {
    (void)user;
    lg__w_keep(text, strlen(text));
    if (!lg__wl.data_mgr || !lg__wl.data_dev) return;
    if (lg__wl.source) wl_data_source_destroy(lg__wl.source);
    lg__wl.source = wl_data_device_manager_create_data_source(lg__wl.data_mgr);
    wl_data_source_add_listener(lg__wl.source, &lg__wl.source_l, 0);
    wl_data_source_offer(lg__wl.source, "text/plain;charset=utf-8");
    wl_data_source_offer(lg__wl.source, "text/plain");
    wl_data_source_offer(lg__wl.source, "UTF8_STRING");
    wl_data_device_set_selection(lg__wl.data_dev, lg__wl.source, lg__wl.serial);
}
static const char *lg__wl_get_clip(void *user) {
    char *text = 0;
    size_t n = 0, cap = 0;
    int fds[2];
    (void)user;
    if (lg__wl.source) return lg__w.clip ? lg__w.clip : "";        /* it is our own text */
    if (!lg__wl.offer || !lg__wl.offer_text || pipe(fds) != 0) return "";
    wl_data_offer_receive(lg__wl.offer, lg__wl.offer_text == 1 ? "text/plain;charset=utf-8" : "text/plain", fds[1]);
    close(fds[1]);
    wl_display_flush(lg__wl.display);
    for (;;) {                  /* the other program writes into the pipe; half a second of silence is the end */
        struct pollfd fd;
        ssize_t got;
        fd.fd = fds[0]; fd.events = POLLIN;
        if (poll(&fd, 1, 500) <= 0) break;
        if (n + 4096 > cap) { cap = cap * 2 + 8192; text = (char *)LOGUIA_REALLOC(text, cap); }
        got = read(fds[0], text + n, 4096);
        if (got <= 0) break;
        n += (size_t)got;
    }
    close(fds[0]);
    lg__w_keep(text ? text : "", n);
    LOGUIA_FREE(text);
    return lg__w.clip;
}

static void lg__wl_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver) {
    (void)d;
    if (strcmp(iface, "wl_compositor") == 0) {
        lg__wl.comp_version = ver < 4 ? (int)ver : 4;
        lg__wl.compositor = (struct wl_compositor *)wl_registry_bind(r, name, &wl_compositor_interface, (uint32_t)lg__wl.comp_version);
    } else if (strcmp(iface, "wl_shm") == 0) lg__wl.shm = (struct wl_shm *)wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (strcmp(iface, "wl_seat") == 0 && !lg__wl.seat) {
        lg__wl.seat = (struct wl_seat *)wl_registry_bind(r, name, &wl_seat_interface, ver < 5 ? ver : 5);
        wl_seat_add_listener(lg__wl.seat, &lg__wl.seat_l, 0);
    } else if (strcmp(iface, "wl_output") == 0 && ver >= 2) {
        wl_output_add_listener((struct wl_output *)wl_registry_bind(r, name, &wl_output_interface, 2), &lg__wl.output_l, 0);
    } else if (strcmp(iface, "wl_data_device_manager") == 0) lg__wl.data_mgr = (struct wl_data_device_manager *)wl_registry_bind(r, name, &wl_data_device_manager_interface, 1);
    else if (strcmp(iface, "xdg_wm_base") == 0) lg__wl.wm_base = (struct wl_proxy *)wl_registry_bind(r, name, &lg__xdg_wm_base_if, 1);
    else if (strcmp(iface, "zxdg_decoration_manager_v1") == 0) lg__wl.deco_mgr = (struct wl_proxy *)wl_registry_bind(r, name, &lg__deco_mgr_if, 1);
    else if (strcmp(iface, "wp_viewporter") == 0) lg__wl.viewporter = (struct wl_proxy *)wl_registry_bind(r, name, &lg__viewporter_if, 1);
    else if (strcmp(iface, "wp_fractional_scale_manager_v1") == 0) lg__wl.frac_mgr = (struct wl_proxy *)wl_registry_bind(r, name, &lg__frac_mgr_if, 1);
    else if (strcmp(iface, "wp_cursor_shape_manager_v1") == 0) lg__wl.shape_mgr = (struct wl_proxy *)wl_registry_bind(r, name, &lg__shape_mgr_if, 1);
    else if (strcmp(iface, "zwp_text_input_manager_v3") == 0) lg__wl.ti_mgr = (struct wl_proxy *)wl_registry_bind(r, name, &lg__ti_mgr_if, 1);
}
static void lg__wl_global_gone(void *d, struct wl_registry *r, uint32_t name) { (void)d; (void)r; (void)name; }

static void lg__wl_close(void) {
    int i;
    if (lg__wl.display) {
        for (i = 0; i < 3; i++) lg__wl_buffer_free(&lg__wl.bufs[i]);
        if (lg__wl.state) xkb_state_unref(lg__wl.state);
        if (lg__wl.keymap) xkb_keymap_unref(lg__wl.keymap);
        if (lg__wl.xkb) xkb_context_unref(lg__wl.xkb);
        if (lg__wl.source) wl_data_source_destroy(lg__wl.source);
        if (lg__wl.offer) wl_data_offer_destroy(lg__wl.offer);
        LOGUIA_FREE(lg__wl.ti_pre); LOGUIA_FREE(lg__wl.ti_commit);
        if (lg__wl.toplevel) wl_proxy_marshal_flags(lg__wl.toplevel, 0, 0, lg__wl_ver(lg__wl.toplevel), WL_MARSHAL_FLAG_DESTROY);
        if (lg__wl.xdg_surface) wl_proxy_marshal_flags(lg__wl.xdg_surface, 0, 0, lg__wl_ver(lg__wl.xdg_surface), WL_MARSHAL_FLAG_DESTROY);
        if (lg__wl.surface) wl_surface_destroy(lg__wl.surface);
        wl_display_flush(lg__wl.display);
        wl_display_disconnect(lg__wl.display);      /* everything else of the connection goes with it */
    }
    memset(&lg__wl, 0, sizeof lg__wl);
}
static int lg__wl_open(const char *title, int width, int height) {
    struct wl_display *d = wl_display_connect(0);
    int tries;
    memset(&lg__wl, 0, sizeof lg__wl);
    if (!d) return 0;
    lg__wl.display = d;
    lg__wl.out_scale = 1; lg__wl.rate = 25; lg__wl.delay = 400;
    lg__wl.lw = width; lg__wl.lh = height;
    lg__wl.registry_l.global = lg__wl_global; lg__wl.registry_l.global_remove = lg__wl_global_gone;
    lg__wl.seat_l.capabilities = lg__wl_seat_caps; lg__wl.seat_l.name = lg__wl_seat_name;
    lg__wl.pointer_l.enter = lg__wl_pointer_enter; lg__wl.pointer_l.leave = lg__wl_pointer_leave;
    lg__wl.pointer_l.motion = lg__wl_pointer_motion; lg__wl.pointer_l.button = lg__wl_pointer_button;
    lg__wl.pointer_l.axis = lg__wl_pointer_axis; lg__wl.pointer_l.frame = lg__wl_pointer_frame;
    lg__wl.pointer_l.axis_source = lg__wl_pointer_source; lg__wl.pointer_l.axis_stop = lg__wl_pointer_stop;
    lg__wl.pointer_l.axis_discrete = lg__wl_pointer_discrete;
    lg__wl.keyboard_l.keymap = lg__wl_keymap; lg__wl.keyboard_l.enter = lg__wl_key_enter; lg__wl.keyboard_l.leave = lg__wl_key_leave;
    lg__wl.keyboard_l.key = lg__wl_key_event; lg__wl.keyboard_l.modifiers = lg__wl_key_mods; lg__wl.keyboard_l.repeat_info = lg__wl_key_repeat;
    lg__wl.output_l.geometry = lg__wl_out_geometry; lg__wl.output_l.mode = lg__wl_out_mode;
    lg__wl.output_l.done = lg__wl_out_done; lg__wl.output_l.scale = lg__wl_out_scale;
    lg__wl.buffer_l.release = lg__wl_buffer_release;
    lg__wl.device_l.data_offer = lg__wl_device_offer; lg__wl.device_l.enter = lg__wl_device_enter; lg__wl.device_l.leave = lg__wl_device_leave;
    lg__wl.device_l.motion = lg__wl_device_motion; lg__wl.device_l.drop = lg__wl_device_drop; lg__wl.device_l.selection = lg__wl_device_selection;
    lg__wl.offer_l.offer = lg__wl_offer_type;
    lg__wl.source_l.target = lg__wl_source_target; lg__wl.source_l.send = lg__wl_source_send; lg__wl.source_l.cancelled = lg__wl_source_cancelled;
    lg__wl.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    lg__wl.registry = wl_display_get_registry(d);
    wl_registry_add_listener(lg__wl.registry, &lg__wl.registry_l, 0);
    wl_display_roundtrip(d);            /* the list of what the compositor offers... */
    wl_display_roundtrip(d);            /* ...and the first news from the seat and the screens */
    if (!lg__wl.compositor || !lg__wl.shm || !lg__wl.wm_base || !lg__wl.xkb) { lg__wl_close(); return 0; }
#ifndef LOGUIA_NO_X11
    if (!lg__wl.deco_mgr) { lg__wl_close(); return 0; }     /* nobody would draw a title bar: better an X11 window */
#endif
    lg__w.scale = (float)lg__wl.out_scale;
    wl_proxy_add_listener(lg__wl.wm_base, lg__wl_base_l, 0);
    lg__wl.surface = wl_compositor_create_surface(lg__wl.compositor);
    lg__wl.xdg_surface = wl_proxy_marshal_flags(lg__wl.wm_base, 2, &lg__xdg_surface_if, 1, 0, (void *)0, lg__wl.surface);
    wl_proxy_add_listener(lg__wl.xdg_surface, lg__wl_surface_l, 0);
    lg__wl.toplevel = wl_proxy_marshal_flags(lg__wl.xdg_surface, 1, &lg__xdg_toplevel_if, 1, 0, (void *)0);
    wl_proxy_add_listener(lg__wl.toplevel, lg__wl_toplevel_l, 0);
    wl_proxy_marshal_flags(lg__wl.toplevel, 2, 0, 1, 0, title);
    wl_proxy_marshal_flags(lg__wl.toplevel, 3, 0, 1, 0, "loguia");
    if (lg__wl.deco_mgr) {              /* ask the compositor to draw the title bar */
        lg__wl.deco = wl_proxy_marshal_flags(lg__wl.deco_mgr, 1, &lg__deco_if, 1, 0, (void *)0, lg__wl.toplevel);
        wl_proxy_add_listener(lg__wl.deco, lg__wl_deco_l, 0);
        wl_proxy_marshal_flags(lg__wl.deco, 1, 0, 1, 0, 2u);
    }
    if (lg__wl.frac_mgr && lg__wl.viewporter) {     /* scales like 125 %: the picture is made at exactly that size */
        lg__wl.frac = wl_proxy_marshal_flags(lg__wl.frac_mgr, 1, &lg__frac_if, 1, 0, (void *)0, lg__wl.surface);
        wl_proxy_add_listener(lg__wl.frac, lg__wl_frac_l, 0);
        lg__wl.viewport = wl_proxy_marshal_flags(lg__wl.viewporter, 1, &lg__viewport_if, 1, 0, (void *)0, lg__wl.surface);
    } else if (lg__wl.comp_version < 3) lg__w.scale = 1;
    if (lg__wl.data_mgr && lg__wl.seat) {
        lg__wl.data_dev = wl_data_device_manager_get_data_device(lg__wl.data_mgr, lg__wl.seat);
        wl_data_device_add_listener(lg__wl.data_dev, &lg__wl.device_l, 0);
    }
    if (lg__wl.ti_mgr && lg__wl.seat) {
        lg__wl.ti = wl_proxy_marshal_flags(lg__wl.ti_mgr, 1, &lg__ti_if, 1, 0, (void *)0, lg__wl.seat);
        wl_proxy_add_listener(lg__wl.ti, lg__wl_ti_l, 0);
    }
    wl_surface_commit(lg__wl.surface);
    for (tries = 0; tries < 20 && !lg__wl.configured; tries++) if (wl_display_roundtrip(d) < 0) break;     /* the compositor says how big the window is */
    if (!lg__wl.configured) { lg__wl_close(); return 0; }
    lg_init();
    lg_set_clipboard(lg__wl_get_clip, lg__wl_set_clip, 0);
    lg__wl_resize();
    lg__wl.last = lg__wl_now();
    lg__w.open = 1;
    return 1;
}
static int lg__wl_begin_frame(void) {
    struct wl_display *d = lg__wl.display;
    double now;
    if (!lg__w.open) return 0;
    for (;;) {
        struct pollfd fd;
        int wait = lg__w.idle > 0 ? 0 : 250, got;
        while (wl_display_prepare_read(d) != 0) wl_display_dispatch_pending(d);
        wl_display_flush(d);
        if (lg__wl.repeat_key && wait > 0) {        /* a held key repeats: wake up in time for it */
            double left = (lg__wl.repeat_at - lg__wl_now()) * 1000;
            if (left < wait) wait = left > 0 ? (int)left + 1 : 0;
        }
        fd.fd = wl_display_get_fd(d); fd.events = POLLIN;
        got = poll(&fd, 1, wait);
        if (got > 0) wl_display_read_events(d); else wl_display_cancel_read(d);
        if (wl_display_dispatch_pending(d) < 0) lg__w.open = 0;        /* the compositor is gone */
        now = lg__wl_now();
        if (lg__wl.repeat_key && now >= lg__wl.repeat_at) {
            lg__wl_press(lg__wl.repeat_key);
            lg__wl.repeat_at = now + 1.0 / (lg__wl.rate > 0 ? lg__wl.rate : 25);
            lg__w.idle = 3;
        }
        if (!lg__w.open) return 0;
        if (lg__w.idle > 0) break;
        if (got == 0) lg__w.idle = 1;               /* nothing happened: one frame, so the caret blinks */
    }
    now = lg__wl_now();
    lg__w_begin((float)(now - lg__wl.last));
    lg__wl.last = now;
    return 1;
}
static void lg__wl_end_frame(unsigned int background_rgba) {
    lg__wl_buf *b;
    lg__w_draw(background_rgba, 1);
    b = lg__wl_buffer(lg__w.w, lg__w.h);
    if (b) {
        memcpy(b->data, lg__w.px, b->size);
        wl_surface_attach(lg__wl.surface, b->buf, 0, 0);
        wl_surface_damage(lg__wl.surface, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
        if (lg__wl.viewport) wl_proxy_marshal_flags(lg__wl.viewport, 2, 0, 1, 0, lg__wl.lw, lg__wl.lh);        /* shown at the window's size, whatever the scale */
        else if (lg__wl.comp_version >= 3) wl_surface_set_buffer_scale(lg__wl.surface, (int32_t)lg__w.scale);
        wl_surface_commit(lg__wl.surface);
        b->busy = 1;
    }
    lg__wl_ime();
    wl_display_flush(lg__wl.display);
}
#endif /* LOGUIA_WAYLAND */

#ifndef LOGUIA_NO_X11
/* ---------------------------------------------------------------- X11 (Linux, BSD; under Wayland through XWayland) */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <locale.h>
#include <stdlib.h>
#include <wchar.h>

static struct {
    Display *dpy;
    Window win;
    GC gc;
    XImage *img;
    XIM im;
    XIC ic;
    XIMCallback pe_cb[4];           /* the input method tells what it is composing (start, done, draw, caret) */
    unsigned int pe[256];
    int pe_len, pe_caret;
    XPoint spot;
    Atom wm_delete, clipboard, utf8, targets, prop;
    int bgr;
    struct timespec last;
} lg__x;

/* Xlib ends the program on any error by default. The ones that can happen here are harmless:
   another program asked for the clipboard and left, or the window was destroyed from outside. */
static int lg__x_error(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

static int lg__x_key(KeySym k) {
    switch (k) {
    case XK_Left: return LG_KEY_LEFT;           case XK_Right: return LG_KEY_RIGHT;
    case XK_Up: return LG_KEY_UP;               case XK_Down: return LG_KEY_DOWN;
    case XK_Home: return LG_KEY_HOME;           case XK_End: return LG_KEY_END;
    case XK_BackSpace: return LG_KEY_BACKSPACE; case XK_Delete: return LG_KEY_DELETE;
    case XK_Return: case XK_KP_Enter: return LG_KEY_ENTER;
    case XK_Tab: case XK_ISO_Left_Tab: return LG_KEY_TAB;
    case XK_Escape: return LG_KEY_ESCAPE;       case XK_space: return LG_KEY_SPACE;
    case XK_Prior: return LG_KEY_PAGE_UP;       case XK_Next: return LG_KEY_PAGE_DOWN;
    case XK_Control_L: case XK_Control_R: return LG_KEY_CTRL;
    case XK_Shift_L: case XK_Shift_R: return LG_KEY_SHIFT;
    case XK_a: return LG_KEY_A;                 case XK_c: return LG_KEY_C;
    case XK_v: return LG_KEY_V;                 case XK_x: return LG_KEY_X;
    case XK_z: return LG_KEY_Z;                 case XK_y: return LG_KEY_Y;
    }
    return LG_KEY_NONE;
}
/* the word being composed, as the input method describes it piece by piece */
static void lg__x_compose(void) {
    char out[256 * 4 + 1];
    int n = 0, i, caret = -1;
    for (i = 0; i < lg__x.pe_len && n < (int)sizeof out - 5; i++) {
        if (i == lg__x.pe_caret) caret = n;
        n += lg__utf8_put(lg__x.pe[i], out + n);
    }
    out[n] = 0;
    lg_input_compose(out, caret);
    lg__w.idle = 3;
}
static int lg__x_pe_start(XIC ic, XPointer user, XPointer data) { (void)ic; (void)user; (void)data; lg__x.pe_len = lg__x.pe_caret = 0; return -1; }
static void lg__x_pe_done(XIC ic, XPointer user, XPointer data) { (void)ic; (void)user; (void)data; lg__x.pe_len = lg__x.pe_caret = 0; lg__x_compose(); }
static void lg__x_pe_draw(XIC ic, XPointer user, XPointer data) {
    XIMPreeditDrawCallbackStruct *d = (XIMPreeditDrawCallbackStruct *)data;
    unsigned int add[256];
    int n = 0, first = d->chg_first, len = d->chg_length, i, keep;
    (void)ic; (void)user;
    if (d->text && d->text->encoding_is_wchar && d->text->string.wide_char)
        for (i = 0; i < d->text->length && n < 256; i++) add[n++] = (unsigned int)d->text->string.wide_char[i];
    else if (d->text && !d->text->encoding_is_wchar && d->text->string.multi_byte) {
        wchar_t wide[256];
        int k = (int)mbstowcs(wide, d->text->string.multi_byte, 256);
        for (i = 0; i < k && n < 256; i++) add[n++] = (unsigned int)wide[i];
    }
    if (first < 0) first = 0;
    if (first > lg__x.pe_len) first = lg__x.pe_len;
    if (len < 0) len = 0;
    if (len > lg__x.pe_len - first) len = lg__x.pe_len - first;
    keep = lg__x.pe_len - first - len;
    if (first + n + keep > 256) n = 256 - first - keep;
    memmove(lg__x.pe + first + n, lg__x.pe + first + len, (size_t)keep * sizeof *lg__x.pe);
    memcpy(lg__x.pe + first, add, (size_t)n * sizeof *add);
    lg__x.pe_len = first + n + keep;
    lg__x.pe_caret = d->caret < 0 ? 0 : d->caret > lg__x.pe_len ? lg__x.pe_len : d->caret;
    lg__x_compose();
}
static void lg__x_pe_caret(XIC ic, XPointer user, XPointer data) {
    XIMPreeditCaretCallbackStruct *c = (XIMPreeditCaretCallbackStruct *)data;
    int at = lg__x.pe_caret;
    (void)ic; (void)user;
    switch (c->direction) {
    case XIMForwardChar: at++; break;
    case XIMBackwardChar: at--; break;
    case XIMLineStart: at = 0; break;
    case XIMLineEnd: at = lg__x.pe_len; break;
    case XIMAbsolutePosition: at = c->position; break;
    default: break;
    }
    lg__x.pe_caret = c->position = at < 0 ? 0 : at > lg__x.pe_len ? lg__x.pe_len : at;
    lg__x_compose();
}
/* the input method's list of candidates goes where the caret is */
static void lg__x_ime(void) {
    float x, y, w, h;
    XPoint at;
    XVaNestedList list;
    if (!lg__x.ic || !lg_ime_rect(&x, &y, &w, &h)) return;
    at.x = (short)(x * lg__w.scale); at.y = (short)((y + h) * lg__w.scale);
    if (at.x == lg__x.spot.x && at.y == lg__x.spot.y) return;
    lg__x.spot = at;
    list = XVaCreateNestedList(0, XNSpotLocation, &at, (char *)0);
    XSetICValues(lg__x.ic, XNPreeditAttributes, list, (char *)0);
    XFree(list);
}
static void lg__x_resize(int w, int h) {
    if (lg__x.img) { lg__x.img->data = 0; XDestroyImage(lg__x.img); }       /* the pixels are ours, not Xlib's */
    lg__w_size(w, h);
    lg__x.img = XCreateImage(lg__x.dpy, DefaultVisual(lg__x.dpy, DefaultScreen(lg__x.dpy)),
                             (unsigned int)DefaultDepth(lg__x.dpy, DefaultScreen(lg__x.dpy)), ZPixmap, 0, (char *)lg__w.px,
                             (unsigned int)lg__w.w, (unsigned int)lg__w.h, 32, 0);
    lg__x.img->byte_order = LSBFirst;
}
/* another program asks for the text we copied */
static void lg__x_give(XSelectionRequestEvent *q) {
    XEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.xselection.type = SelectionNotify;
    ev.xselection.requestor = q->requestor; ev.xselection.selection = q->selection;
    ev.xselection.target = q->target; ev.xselection.time = q->time;
    ev.xselection.property = q->property ? q->property : q->target;
    if (q->target == lg__x.targets) {
        Atom list[3];
        list[0] = lg__x.targets; list[1] = lg__x.utf8; list[2] = XA_STRING;
        XChangeProperty(lg__x.dpy, q->requestor, ev.xselection.property, XA_ATOM, 32, PropModeReplace, (unsigned char *)list, 3);
    } else if ((q->target == lg__x.utf8 || q->target == XA_STRING) && lg__w.clip) {
        XChangeProperty(lg__x.dpy, q->requestor, ev.xselection.property, q->target, 8, PropModeReplace,
                        (unsigned char *)lg__w.clip, (int)strlen(lg__w.clip));
    } else ev.xselection.property = None;
    XSendEvent(lg__x.dpy, q->requestor, False, 0, &ev);
}
static void lg__x_event(XEvent *ev) {
    lg__w.idle = 3;
    switch (ev->type) {
    case ClientMessage: if ((Atom)ev->xclient.data.l[0] == lg__x.wm_delete) lg__w.open = 0; break;
    case ConfigureNotify: if (ev->xconfigure.width != lg__w.w || ev->xconfigure.height != lg__w.h) lg__x_resize(ev->xconfigure.width, ev->xconfigure.height); break;
    case MotionNotify: lg_input_mouse_pos((float)ev->xmotion.x / lg__w.scale, (float)ev->xmotion.y / lg__w.scale); break;
    case ButtonPress: case ButtonRelease: {
        int b = (int)ev->xbutton.button, down = ev->type == ButtonPress;
        lg_input_mouse_pos((float)ev->xbutton.x / lg__w.scale, (float)ev->xbutton.y / lg__w.scale);
        if (b == 4 || b == 5) { if (down) lg_input_scroll(0, b == 4 ? 1.0f : -1.0f); }      /* the wheel is buttons 4 and 5 */
        else if (b >= 1 && b <= 3) lg_input_mouse_button(b == 1 ? 0 : b == 3 ? 1 : 2, down);
        break;
    }
    case KeyPress: {
        char text[64];
        const char *p = text, *end;
        int n;
        KeySym sym = 0;
        Status status;
        lg_input_key(lg__x_key(XLookupKeysym(&ev->xkey, 0)), 1);
        if (lg__x.ic) n = Xutf8LookupString(lg__x.ic, &ev->xkey, text, (int)sizeof text - 1, &sym, &status);
        else n = XLookupString(&ev->xkey, text, (int)sizeof text - 1, &sym, 0);
        if (n < 0 || n >= (int)sizeof text) n = 0;
        for (end = text + n; p < end;) { unsigned int cp; p = lg__utf8(p, end, &cp); lg_input_char(cp); }
        break;
    }
    case KeyRelease: lg_input_key(lg__x_key(XLookupKeysym(&ev->xkey, 0)), 0); break;
    case MappingNotify: XRefreshKeyboardMapping(&ev->xmapping); break;      /* the keyboard layout changed */
    case DestroyNotify: lg__w.open = 0; break;
    case FocusIn: if (lg__x.ic) XSetICFocus(lg__x.ic); break;
    case FocusOut:
        lg_input_key(LG_KEY_CTRL, 0); lg_input_key(LG_KEY_SHIFT, 0);
        if (lg__x.ic) XUnsetICFocus(lg__x.ic);
        break;
    case SelectionRequest: lg__x_give(&ev->xselectionrequest); break;
    }
}
static void lg__x_set_clip(void *user, const char *text) {
    (void)user;
    lg__w_keep(text, strlen(text));
    XSetSelectionOwner(lg__x.dpy, lg__x.clipboard, lg__x.win, CurrentTime);
}
static const char *lg__x_get_clip(void *user) {
    XEvent ev;
    int tries;
    (void)user;
    if (XGetSelectionOwner(lg__x.dpy, lg__x.clipboard) == lg__x.win) return lg__w.clip ? lg__w.clip : "";
    XConvertSelection(lg__x.dpy, lg__x.clipboard, lg__x.utf8, lg__x.prop, lg__x.win, CurrentTime);
    XFlush(lg__x.dpy);
    for (tries = 0; tries < 50; tries++) {          /* the other program answers within half a second, or we give up */
        struct pollfd fd;
        if (XCheckTypedWindowEvent(lg__x.dpy, lg__x.win, SelectionNotify, &ev)) {
            Atom type;
            int format;
            unsigned long n, left;
            unsigned char *data = 0;
            if (ev.xselection.property == None) return "";
            if (XGetWindowProperty(lg__x.dpy, lg__x.win, lg__x.prop, 0, 1 << 20, True, AnyPropertyType, &type, &format, &n, &left, &data) != Success || !data) return "";
            if (format == 8) lg__w_keep((const char *)data, n); else lg__w_keep("", 0);
            XFree(data);
            return lg__w.clip;
        }
        fd.fd = ConnectionNumber(lg__x.dpy); fd.events = POLLIN;
        poll(&fd, 1, 10);
    }
    return "";
}
static int lg__x_open(const char *title, int width, int height) {
    Display *d;
    Visual *vis;
    const char *res, *dpi;
    int screen;
    memset(&lg__w, 0, sizeof lg__w);
    memset(&lg__x, 0, sizeof lg__x);
    { const char *loc = setlocale(LC_CTYPE, 0); if (!loc || strcmp(loc, "C") == 0) setlocale(LC_CTYPE, ""); }     /* typed text in the user's language */
    d = lg__x.dpy = XOpenDisplay(0);
    if (!d) return 0;
    XSetErrorHandler(lg__x_error);
    screen = DefaultScreen(d);
    vis = DefaultVisual(d, screen);
    if (DefaultDepth(d, screen) < 24 || (vis->red_mask != 0xFF0000 && vis->red_mask != 0xFF)) { XCloseDisplay(d); return 0; }
    lg__x.bgr = vis->red_mask == 0xFF0000;
    lg__w.scale = 1;
    res = XResourceManagerString(d);            /* "Xft.dpi: 144" is how desktops announce a scaled screen */
    dpi = res ? strstr(res, "Xft.dpi:") : 0;
    if (dpi) {
        float v = 0;
        for (dpi += 8; *dpi == ' ' || *dpi == '\t'; dpi++) {}
        for (; *dpi >= '0' && *dpi <= '9'; dpi++) v = v * 10 + (float)(*dpi - '0');
        if (v >= 96 && v <= 960) lg__w.scale = v / 96;
    }
    lg__x.win = XCreateSimpleWindow(d, RootWindow(d, screen), 0, 0, (unsigned int)((float)width * lg__w.scale),
                                    (unsigned int)((float)height * lg__w.scale), 0, 0, 0);
    XSelectInput(d, lg__x.win, ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask | FocusChangeMask);
    lg__x.wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    lg__x.clipboard = XInternAtom(d, "CLIPBOARD", False);
    lg__x.utf8 = XInternAtom(d, "UTF8_STRING", False);
    lg__x.targets = XInternAtom(d, "TARGETS", False);
    lg__x.prop = XInternAtom(d, "LOGUIA_PASTE", False);
    XSetWMProtocols(d, lg__x.win, &lg__x.wm_delete, 1);
    XStoreName(d, lg__x.win, title);
    XChangeProperty(d, lg__x.win, XInternAtom(d, "_NET_WM_NAME", False), lg__x.utf8, 8, PropModeReplace, (const unsigned char *)title, (int)strlen(title));
    lg__x.gc = XCreateGC(d, lg__x.win, 0, 0);
    XSetLocaleModifiers("");
    lg__x.im = XOpenIM(d, 0, 0, 0);
    if (lg__x.im) {
        /* Input methods (Chinese, Japanese, Korean...): best when they tell us what they are composing,
           so Loguia shows it at the caret; otherwise they show it in a little window of their own. */
        XIMStyles *styles = 0;
        int told = 0, i;
        long mask = 0;
        if (XGetIMValues(lg__x.im, XNQueryInputStyle, &styles, (char *)0) == 0 && styles) {
            for (i = 0; i < styles->count_styles; i++) if (styles->supported_styles[i] == (XIMPreeditCallbacks | XIMStatusNothing)) told = 1;
            XFree(styles);
        }
        if (told) {
            XVaNestedList pe;
            lg__x.pe_cb[0].callback = (XIMProc)(void (*)(void))lg__x_pe_start;
            lg__x.pe_cb[1].callback = (XIMProc)(void (*)(void))lg__x_pe_done;      /* they get the XIC, whatever XIMProc says */
            lg__x.pe_cb[2].callback = (XIMProc)(void (*)(void))lg__x_pe_draw;
            lg__x.pe_cb[3].callback = (XIMProc)(void (*)(void))lg__x_pe_caret;
            pe = XVaCreateNestedList(0, XNPreeditStartCallback, &lg__x.pe_cb[0], XNPreeditDoneCallback, &lg__x.pe_cb[1],
                                     XNPreeditDrawCallback, &lg__x.pe_cb[2], XNPreeditCaretCallback, &lg__x.pe_cb[3], (char *)0);
            lg__x.ic = XCreateIC(lg__x.im, XNInputStyle, XIMPreeditCallbacks | XIMStatusNothing, XNClientWindow, lg__x.win,
                                 XNFocusWindow, lg__x.win, XNPreeditAttributes, pe, (char *)0);
            XFree(pe);
        }
        if (!lg__x.ic) lg__x.ic = XCreateIC(lg__x.im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, lg__x.win, XNFocusWindow, lg__x.win, (char *)0);
        if (lg__x.ic) {             /* the input method may want to see more events than the window asked for */
            XGetICValues(lg__x.ic, XNFilterEvents, &mask, (char *)0);
            XSelectInput(d, lg__x.win, ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                         ButtonReleaseMask | PointerMotionMask | FocusChangeMask | mask);
            XSetICFocus(lg__x.ic);
        }
    }
    lg_init();
    lg_set_clipboard(lg__x_get_clip, lg__x_set_clip, 0);
    lg__x_resize((int)((float)width * lg__w.scale), (int)((float)height * lg__w.scale));
    XMapWindow(d, lg__x.win);
    clock_gettime(CLOCK_MONOTONIC, &lg__x.last);
    lg__w.open = 1;
    return 1;
}
static int lg__x_begin_frame(void) {
    struct timespec now;
    XEvent ev;
    if (!lg__w.open) return 0;
    for (;;) {
        while (XPending(lg__x.dpy)) {
            XNextEvent(lg__x.dpy, &ev);
            if (!XFilterEvent(&ev, None)) lg__x_event(&ev);
        }
        if (!lg__w.open) return 0;
        if (lg__w.idle > 0) break;
        {
            struct pollfd fd;
            fd.fd = ConnectionNumber(lg__x.dpy); fd.events = POLLIN;
            if (poll(&fd, 1, 250) == 0) lg__w.idle = 1;        /* nothing happened: one frame, so the caret blinks */
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    lg__w_begin((float)(now.tv_sec - lg__x.last.tv_sec) + (float)(now.tv_nsec - lg__x.last.tv_nsec) / 1e9f);
    lg__x.last = now;
    return 1;
}
static void lg__x_end_frame(unsigned int background_rgba) {
    lg__w_draw(background_rgba, lg__x.bgr);
    XPutImage(lg__x.dpy, lg__x.win, lg__x.gc, lg__x.img, 0, 0, 0, 0, (unsigned int)lg__w.w, (unsigned int)lg__w.h);
    lg__x_ime();
    XFlush(lg__x.dpy);
}
static void lg__x_close(void) {
    if (lg__x.dpy) {
        if (lg__x.img) { lg__x.img->data = 0; XDestroyImage(lg__x.img); }
        if (lg__x.ic) XDestroyIC(lg__x.ic);
        if (lg__x.im) XCloseIM(lg__x.im);
        XFreeGC(lg__x.dpy, lg__x.gc);
        XDestroyWindow(lg__x.dpy, lg__x.win);
        XCloseDisplay(lg__x.dpy);
    }
    memset(&lg__x, 0, sizeof lg__x);
}
#endif /* X11 */

/* Wayland when it was asked for and is there, X11 otherwise */
LGDEF int lg_window_open(const char *title, int width, int height) {
    memset(&lg__w, 0, sizeof lg__w);
#ifdef LOGUIA_WAYLAND
    if (lg__wl_open(title, width, height)) return 1;
#endif
#ifndef LOGUIA_NO_X11
    return lg__x_open(title, width, height);
#else
    (void)title; (void)width; (void)height;
    return 0;
#endif
}
LGDEF int lg_window_begin_frame(void) {
#ifdef LOGUIA_WAYLAND
    if (lg__wl.display) return lg__wl_begin_frame();
#endif
#ifndef LOGUIA_NO_X11
    return lg__x_begin_frame();
#else
    return 0;
#endif
}
LGDEF void lg_window_end_frame(unsigned int background_rgba) {
#ifdef LOGUIA_WAYLAND
    if (lg__wl.display) { lg__wl_end_frame(background_rgba); return; }
#endif
#ifndef LOGUIA_NO_X11
    lg__x_end_frame(background_rgba);
#else
    (void)background_rgba;
#endif
}
LGDEF void lg_window_close(void) {
#ifdef LOGUIA_WAYLAND
    lg__wl_close();
#endif
#ifndef LOGUIA_NO_X11
    lg__x_close();
#endif
    LOGUIA_FREE(lg__w.px); LOGUIA_FREE(lg__w.clip);
    memset(&lg__w, 0, sizeof lg__w);
    lg_shutdown();
}
#endif
#endif /* LOGUIA_WINDOW */

#endif /* LOGUIA_IMPLEMENTATION */
