<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <img alt="Loguia" src="assets/logo.svg" width="420">
  </picture>
</p>

<p align="center"><b>A complete immediate-mode GUI in one C file.</b><br>
Docking, menus, tables, a code editor, any font, every input method, native windows on Windows, macOS, X11 and Wayland. No dependencies.</p>

---

Copy `loguia.h` into your project. That is the whole installation: no build system, no libraries, the font is inside the file.

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

There are no widget objects to create and keep in sync. Every frame you describe the interface with plain function calls, and a call like `lg_button` answers straight away whether it was clicked.

## A whole program

```c
#define LOGUIA_IMPLEMENTATION
#define LOGUIA_WINDOW
#include "loguia.h"

int main(void) {
    if (!lg_window_open("Hello", 400, 300)) return 1;
    while (lg_window_begin_frame()) {          /* sleeps while nothing happens */
        if (lg_begin("Hello", 20, 20, 300, 200, 0)) lg_text("Hello, world");
        lg_end();
        lg_window_end_frame(0x0E1116FF);
    }
    lg_window_close();
    return 0;
}
```

```sh
cc hello.c -lX11                                              # Linux, BSD
cc -DLOGUIA_WAYLAND hello.c -lwayland-client -lxkbcommon -lX11  # Linux, BSD, as a native Wayland window
cc hello.c -lgdi32                                            # Windows
cc hello.c -framework Cocoa                                   # macOS
```

## Modules

The core is plain C99 and never talks to the system: mouse and keyboard go in, a list of coloured triangles comes out. Everything around it is optional and switched on with one `#define`:

| Define | What it gives |
|---|---|
| `LOGUIA_WINDOW` | `lg_window_*`: a native window with its input, clipboard, input methods and drawing. Win32 on Windows, Cocoa on macOS, X11 on Linux and BSD |
| `LOGUIA_WAYLAND` | with `LOGUIA_WINDOW`: a native Wayland window, sharp at fractional scales like 125 % and 150 %, falling back to X11 where Wayland is not there. `LOGUIA_NO_X11` drops X11 entirely |
| `LOGUIA_GL` | `lg_gl_render`: drawing with OpenGL |
| `LOGUIA_GLFW` | `lg_glfw_*`: input, clipboard and timing from a GLFW window |
| `LOGUIA_SOFTWARE` | `lg_soft_render`: drawing into an RGBA pixel buffer, no GPU needed |

Already have a window and a renderer (SDL, raylib, Vulkan, a game engine)? Use none of them: feed the `lg_input_*` functions and draw the triangle list yourself. `lg_gl_render` is a 30-line example of a renderer.

## Widgets

Windows that move, resize, stack and scroll; scrolling panels; text and wrapped text; buttons, checkboxes, sliders (click the number to type a value); text fields and a multi-line editor with line numbers, undo and redo, word wrap and a read-only mode; combo boxes, popups and tooltips; tab bars; tables with columns the user resizes and sorts; menu bars with nested menus; tree nodes, collapsing headers, progress bars, selectable rows and images.

```c
static const char *langs[] = { "C", "Finch", "Rust" };
static const char *tabs[] = { "Files", "Settings" };

lg_tabs("tabs", tabs, 2, &tab);                       /* then show what belongs to tabs[tab] */
lg_combo("Language", &lang, langs, 3);
lg_input_text_multiline("##code", code, sizeof code, 0, 200, LG_EDIT_WRAP);

if (lg_button("Menu")) lg_open_popup("menu");
lg_tooltip("Shown while the mouse rests on the button");
if (lg_begin_popup("menu")) {
    if (lg_selectable("Save", 0)) save();
    lg_end_popup();
}

lg_begin_table("files", 2, NULL, LG_TABLE_BORDERS | LG_TABLE_HEADER | LG_TABLE_RESIZE | LG_TABLE_SORT);
if (lg_table_sort(&column, &descending)) sort_rows(column, descending);
lg_table_next(); lg_text("Name");   lg_table_next(); lg_text("Size");
lg_table_next(); lg_text("a.fch");  lg_table_next(); lg_text("12 kB");
lg_end_table();
```

### Menus

A menu bar belongs to the window it is called in. Called outside every window, it is the main menu bar along the top of the screen.

```c
if (lg_begin_menu_bar()) {
    if (lg_begin_menu("File")) {
        if (lg_menu_item("Open...", "Ctrl+O", 0)) open_file();
        if (lg_begin_menu("Recent")) { ...; lg_end_menu(); }
        lg_end_menu();
    }
    if (lg_begin_menu("View")) {
        if (lg_menu_item("Word wrap", NULL, wrap)) wrap = !wrap;      /* the last argument draws a check mark */
        lg_end_menu();
    }
    lg_end_menu_bar();
}
```

### Docking

Give a window `LG_WINDOW_DOCK` and the user can drag it by its title onto the squares that appear: at an edge of the screen, or beside, above, below or onto another window, where they become tabs. The gaps between docked windows can be dragged. Two floating windows dropped onto each other become a group that floats together. Pulling a title or tab away makes the window float again. It is a choice per window: windows without the flag stay where they are.

```c
lg_dock_window("Files", NULL, LG_DOCK_LEFT, 0.2f);        /* a starting layout, from code */
lg_dock_window("Output", NULL, LG_DOCK_BOTTOM, 0.3f);
lg_dock_window("Problems", "Output", LG_DOCK_TAB, 0);

if (lg_begin("Files", 0, 0, 300, 400, LG_WINDOW_DOCK)) { ... }    /* 0: a tab behind another one */
lg_end();

lg_dock_space(&x, &y, &w, &h);                 /* the free middle, for your own drawing */
lg_dock_save(text, sizeof text);               /* the layout the user made, as text to keep in a file */
lg_dock_load(text);                            /* and back, on the next start */
```

### Fonts

The built-in font is JetBrains Mono, with a double-size copy for HiDPI screens. Any TrueType or OpenType font (`.ttf`, `.otf`, `.ttc`, variable fonts included) can be loaded at any size while the program runs. Characters are drawn the first time they appear, so a Chinese, Japanese or Korean font with tens of thousands of them costs only what is on screen. A second font can stand behind the first for the characters it lacks.

```c
lg_font *cjk = lg_font_load_file("NotoSansCJK.ttc", 16);
lg_font *latin = lg_font_load_file("Inter.ttf", 16);
lg_font_fallback(cjk, latin);                  /* what the first font lacks comes from the second */
lg_set_font(cjk);
```

### Input methods

Chinese, Japanese, Korean and every other language typed through an input method work in every text field. The word being composed shows at the caret, and the system's list of candidates opens next to it. With `LOGUIA_WINDOW` this is automatic on all systems: XIM on X11, text-input on Wayland, IMM on Windows, NSTextInputClient on macOS. With your own window code, pass the composing text to `lg_input_compose` and ask `lg_ime_rect` where the caret is.

### Keyboard

Tab and Shift+Tab (or Up and Down) walk through the widgets, Enter or Space presses, Left and Right move sliders and open or close tree nodes, Escape closes a popup. In text: arrows, Home, End, PageUp and PageDown move, Shift selects, Ctrl+A, C, X and V do what they always do, Ctrl+Z takes a change back and Ctrl+Y brings it again. On macOS, Command does what Ctrl does.

## Make it yours

- **Look**: `lg_get_style()` returns every colour and size: padding, spacing, indent, borders, corner rounding, font scale. `lg_style_dark()` and `lg_style_light()` are starting points. Colours are written `0xRRGGBBAA`.
- **HiDPI**: `lg_set_pixel_scale(1.5)` keeps text and curves sharp on scaled screens. The window and GLFW modules set it themselves.
- **Memory**: `LOGUIA_MALLOC`, `LOGUIA_REALLOC`, `LOGUIA_FREE`.
- **Size**: `LOGUIA_NO_DEFAULT_FONT`, `LOGUIA_NO_FONT_2X` and `LOGUIA_NO_STDIO` leave parts out; `LOGUIA_STATIC` keeps everything private to one file.
- **Limits**: `LOGUIA_MAX_BOXES`, `LOGUIA_MAX_DOCKS`, `LOGUIA_MAX_COLUMNS`, `LOGUIA_MAX_POPUPS`, `LOGUIA_UNDO_LEVELS` and a few more, all listed at the top of `loguia.h`.
- **Your own widgets**: `lg_reserve` takes space in the layout, `lg_interact` handles the mouse, `lg_draw_*` paints.

Every function has a one-line description next to it in `loguia.h`.

## Examples

```sh
cd examples
cc demo_window.c -o demo_window -lX11 && ./demo_window      # every widget in one window
./demo_window NotoSansCJK.ttc Inter.ttf                     # the same with fonts of your choice
cc demo_glfw.c -o demo -lglfw -lGL                          # through GLFW and OpenGL
cc -std=c99 headless.c -o headless && mkdir -p out && ./headless out   # no window: plays the mouse and keyboard, checks every widget, saves pictures
```

`headless.c` is also Loguia's test suite. Give it font files as further arguments to test fonts too.

## Licence

Apache License 2.0, see [LICENSE](LICENSE). The built-in font is JetBrains Mono, under the SIL Open Font License 1.1.
