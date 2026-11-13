# Loguia

A tiny immediate-mode GUI in **one C file**. Copy `loguia.h` into a project and that is the whole installation: no build system, no dependencies, the font is inside the file.

```c
#define LOGUIA_IMPLEMENTATION      /* in exactly one .c file */
#include "loguia.h"

if (lg_begin("Hello", 20, 20, 300, 200, 0)) {
    lg_text("Some text");
    if (lg_button("Press me")) puts("pressed");
    lg_checkbox("Enabled", &enabled);
    lg_slider("Volume", &volume, 0, 1);
    lg_input_text("Name", name, sizeof name);
}
lg_end();
```

"Immediate mode" means there are no widget objects to create and keep in sync. Every frame you describe the interface with plain function calls, and a call like `lg_button` answers straight away whether it was clicked.

## How it is built

The **core** is plain C99 and never talks to the system. You tell it what the mouse and keyboard did, and after `lg_end_frame()` it hands back a list of coloured triangles (`lg_get_draw_list()`). That is why it runs anywhere C runs.

Around the core are **optional modules**, each switched on with one `#define`:

| Define | Gives you | Works on |
|---|---|---|
| `LOGUIA_GL` | `lg_gl_init`, `lg_gl_render`: drawing with OpenGL 1.1 | any compatibility / legacy OpenGL context |
| `LOGUIA_GLFW` | `lg_glfw_init`, `lg_glfw_begin_frame`: mouse, keyboard, clipboard, timing | Linux and BSD (X11 and Wayland), macOS, Windows |
| `LOGUIA_SOFTWARE` | `lg_soft_render`: drawing into an RGBA pixel buffer | everything, no GPU needed |
| `LOGUIA_WINDOW` | `lg_window_*`: a native window, its input, the clipboard and the drawing, with no library at all | Windows (Win32), macOS (Cocoa), Linux and BSD (X11, also under Wayland through XWayland) |
| `LOGUIA_WAYLAND` | with `LOGUIA_WINDOW`: a real Wayland window, sharp at 125 % or 150 % scaling; X11 is used only when there is no Wayland or the compositor draws no title bars (GNOME). `LOGUIA_NO_X11` leaves X11 out | Linux and BSD |

With `LOGUIA_WINDOW` a whole program is this, linked with nothing but `-lX11` (`-lgdi32` on Windows, `-framework Cocoa` on macOS; with `LOGUIA_WAYLAND` add `-lwayland-client -lxkbcommon`):

```c
#define LOGUIA_IMPLEMENTATION
#define LOGUIA_WINDOW
#include "loguia.h"

int main(void) {
    if (!lg_window_open("Hello", 400, 300)) return 1;
    while (lg_window_begin_frame()) {          /* sleeps while nothing happens */
        if (lg_begin("Hello", 20, 20, 300, 200, 0)) lg_text("No GLFW, no OpenGL.");
        lg_end();
        lg_window_end_frame(0x0E1116FF);
    }
    lg_window_close();
    return 0;
}
```

Using something else (SDL, raylib, Vulkan, your own window code)? Skip the modules: feed the `lg_input_*` functions and draw the triangle list yourself. `lg_gl_render` in `loguia.h` is a 30-line example of a renderer.

## What you can change

- **Look**: `lg_get_style()` returns every colour and every size (padding, spacing, indent, borders, corner rounding, font scale). `lg_style_dark()` and `lg_style_light()` are ready-made starting points. Colours are written `0xRRGGBBAA`.
- **HiDPI**: `lg_set_pixel_scale(1.5)` says how many screen pixels one unit of the interface covers. Text is then placed on whole pixels and, from 1.25 up, drawn with a built-in double-size copy of the font, so it stays sharp. The GLFW and window modules call it for you. `LOGUIA_NO_FONT_2X` leaves the big copy out.
- **Font**: `lg_set_font()` takes your own font picture. `tools/bake_font.c` turns any `.ttf` into the block embedded in `loguia.h`. Define `LOGUIA_NO_DEFAULT_FONT` to leave the built-in one out.
- **Memory**: `LOGUIA_MALLOC`, `LOGUIA_REALLOC`, `LOGUIA_FREE`.
- **Limits**: `LOGUIA_MAX_BOXES`, `LOGUIA_STATE_SLOTS`, `LOGUIA_ID_STACK`, `LOGUIA_CLIP_STACK`, `LOGUIA_MAX_POPUPS`, `LOGUIA_MAX_COLUMNS`, `LOGUIA_MAX_DOCKS`, `LOGUIA_UNDO_LEVELS`.
- **Keyboard**: `lg_set_keyboard_nav(0)` turns keyboard navigation off.
- **Other**: `LOGUIA_STATIC`, `LOGUIA_ASSERT`, `LOGUIA_NO_STDIO`, `LOGUIA_GL_HEADER`.
- **Your own widgets**: `lg_reserve` takes space in the layout, `lg_interact` handles the mouse, `lg_draw_*` paints.

The comment at the top of `loguia.h` lists all of this, and every function has a one-line description next to it.

## What is in it

Windows (move, resize, stack, scroll, dock), menu bars with menus inside menus, scrolling panels, text, wrapped text, buttons, checkboxes, sliders (click the number to type one), one-line text fields and many-line editors (line numbers, selection, clipboard, undo and redo, word wrap, UTF-8), combo boxes, popups and popup menus, tooltips, tab bars, tables (columns the user can resize and sort by), progress bars, selectable rows, tree nodes, collapsing headers, images, same-line layout, an ID stack. Corners are rounded and smoothed (`rounding` in the style; 0 gives square ones). The built-in font covers ASCII, Latin-1 and Latin Extended-A, so Polish, Czech, German, French and so on display correctly.

```c
static const char *langs[] = { "C", "Finch", "Rust" };
static const char *tabs[] = { "Files", "Settings" };

lg_tabs("tabs", tabs, 2, &tab);                       /* then show what belongs to tabs[tab] */
lg_combo("Language", &lang, langs, 3);
lg_input_text_multiline("##code", code, sizeof code, 0, 200, LG_EDIT_WRAP);    /* or 0, LG_EDIT_READ_ONLY, LG_EDIT_NO_LINE_NUMBERS */

if (lg_button("Menu")) lg_open_popup("menu");
lg_tooltip("Shown while the mouse rests on the button");
if (lg_begin_popup("menu")) {
    if (lg_selectable("Save", 0)) save();             /* picking a row closes the popup */
    lg_end_popup();
}

lg_begin_table("files", 2, NULL, LG_TABLE_BORDERS | LG_TABLE_HEADER | LG_TABLE_RESIZE | LG_TABLE_SORT);
if (lg_table_sort(&column, &descending)) sort_my_rows(column, descending);    /* Loguia remembers the choice, you sort */
lg_table_next(); lg_text("Name");   lg_table_next(); lg_text("Size");
lg_table_next(); lg_text("a.fch");  lg_table_next(); lg_text("12 kB");
lg_end_table();
```

**Menus**: a menu bar belongs to the window it is called in; called outside every window it is the main menu bar along the top of the screen.

```c
if (lg_begin_menu_bar()) {
    if (lg_begin_menu("File")) {
        if (lg_menu_item("Open...", "Ctrl+O", 0)) open_file();
        if (lg_begin_menu("Recent")) { ...; lg_end_menu(); }      /* a menu inside a menu */
        lg_end_menu();
    }
    if (lg_begin_menu("View")) {
        if (lg_menu_item("Word wrap", NULL, wrap)) wrap = !wrap;  /* the last argument draws a check mark */
        lg_end_menu();
    }
    lg_end_menu_bar();
}
```

**Docking**: a window made with `LG_WINDOW_DOCK` can be dragged by its title onto one of the squares that appear then: at an edge of the screen, or beside, above, below or onto (as a tab) a window that is already docked. The gaps between docked windows can be dragged; pulling a title or a tab away makes the window float again. Dropping a window onto another floating one makes the two a group that floats: the strip along its top moves it, its corner resizes it. Windows without the flag are left alone, so it is a choice per window. A starting layout can be made from code, and `lg_dock_space` tells where the free middle is, for your own drawing:

```c
lg_dock_window("Files", NULL, LG_DOCK_LEFT, 0.2f);        /* once, before the first frame */
lg_dock_window("Output", NULL, LG_DOCK_BOTTOM, 0.3f);
lg_dock_window("Problems", "Output", LG_DOCK_TAB, 0);     /* a tab next to Output */

if (lg_begin("Files", 0, 0, 300, 400, LG_WINDOW_DOCK)) { ... }    /* 0: it is a tab behind another one */
lg_end();
```

The layout the user made can be kept between runs. Loguia turns it into a short text; where the text is kept is up to you:

```c
char text[4096];
lg_dock_save(text, sizeof text);            /* at the end: write text to a file */
lg_dock_load(text);                          /* at the start, after lg_init: read it back */
```

A damaged text is refused and leaves nothing docked.

**Keyboard**: Tab and Shift+Tab (or Up and Down) walk through the widgets, Enter or Space presses the marked one (on a slider: lets you type its number), Left and Right move sliders and open or close tree nodes, Escape closes a popup. Inside a many-line editor Tab types four spaces; Escape leaves the editor. In text: Ctrl+Z takes a change back, Ctrl+Y or Ctrl+Shift+Z brings it again.

Not there yet: input methods for Chinese, Japanese and Korean; fonts drawn at any size at run time (HiDPI uses a second, double-size copy of the font).

## Examples

```sh
cd examples

# no window, no GPU: plays a scripted mouse and keyboard, checks the results, saves PNG pictures
cc -std=c99 headless.c -o headless && ./headless .

# a real window with no library: Loguia's own window code and software drawing
cc demo_window.c -o demo_window -lX11                       # Linux / BSD
cc -DLOGUIA_WAYLAND demo_window.c -o demo_window -lwayland-client -lxkbcommon -lX11     # the same, as a real Wayland window
cc demo_window.c -o demo_window.exe -lgdi32                 # Windows
cc demo_window.c -o demo_window -framework Cocoa            # macOS

# a real window through GLFW and OpenGL
cc demo_glfw.c -o demo -lglfw -lGL                          # Linux / BSD
cc demo_glfw.c -o demo -lglfw -framework OpenGL             # macOS
cc demo_glfw.c -o demo.exe -lglfw3 -lopengl32 -lgdi32       # Windows
```

## What has been tested

- Linux: `headless` clicks, drags, types and scrolls through every widget (typed slider values, undo, menus, sorting and resizing table columns, word wrap, docking with the mouse and from code, floating groups, saving and loading the layout included) and passes under AddressSanitizer and UBSan. `demo_window` (X11) was driven with a simulated mouse and keyboard on a virtual X display.
- Wayland: `demo_window` built with `LOGUIA_WAYLAND` was run in a headless Sway compositor with a virtual mouse and keyboard: clicks, the wheel, dragging a window onto a dock square, Polish text, key repeat, copy and paste in both directions with other programs, closing the window, a screen scaled to 150 %. Not looked at by a person on KDE or GNOME yet; on GNOME it deliberately uses X11.
- Windows: everything compiles with MinGW-w64 without warnings. `headless.exe` passes and `demo_window.exe` works (clicks, typing, undo, menus) under Wine. Not run on a real Windows machine yet.
- macOS: the Cocoa window code has **never been compiled or run on a Mac**. It was only checked for C mistakes against stand-in headers on Linux, so expect to fix something on the first real build. `LOGUIA_GLFW` is the tried way there.
- BSD: written for it (X11, Wayland), not compiled or run there yet.
- HiDPI: checked with the software module at scale 2 and on Wayland at 1.5. The GLFW path on a scaled screen has not been looked at by a person yet.
- Compiles as C99 and as C++ without warnings (`-Wall -Wextra -pedantic`).

## From Finch

Finch imports the header like any C header. See `../Finch_Blocks`:

```
import "loguia.h"

if lg_button("Print") != 0 { ... }
```

## Licence

Apache License 2.0, see `LICENSE`. Copyright 2026 Nexoniarz.

The built-in font is JetBrains Mono, 15 px, under the SIL Open Font License 1.1.
