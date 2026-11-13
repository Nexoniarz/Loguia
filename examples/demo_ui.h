/* The demo interface, shared by demo_glfw.c (a real window) and headless.c (pictures + self-test).
 * Everything Loguia can do is used here once. */
#ifndef DEMO_UI_H
#define DEMO_UI_H
#include "../loguia.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int clicks, check, count, entered, selected, tree_open, light;
    float volume;
    char name[64];
    /* where things ended up on screen: headless.c aims its fake mouse with these */
    float at_button_x, at_button_y, at_check_x, at_check_y, at_slider_x, at_slider_y;
    float at_input_x, at_input_y, at_tree_x, at_tree_y, at_panel_x, at_panel_y;
    float win2_x, win2_y, win2_first_x, win2_now_x, win2_now_y, panel_first_y;
    int win2_moved, panel_scrolled, frames;
    /* the "More" window */
    int tab, language, menu_pick, done[4];
    char code[1024];
    float at_tabs_x, at_tabs_y, at_combo_x, at_combo_y, at_menu_x, at_menu_y, at_editor_x, at_editor_y;
    /* the menu bar, the editor's switches, the table */
    int wrap, no_numbers, file_pick, quit, sort_column, sort_down, first_row;
    float at_file_x, at_file_y, at_view_x, at_table_x, at_table_y, at_head_x[3], at_count_x, at_count_y, at_count_w;
    float at_recent_x, at_recent_y;
    char layout[4096];          /* what View > Remember the layout wrote */
    int style_shown, more_shown;
} demo_state;

static void demo_init(demo_state *s) {
    memset(s, 0, sizeof *s);
    s->check = 1;
    s->volume = 0.4f;
    s->count = 3;
    s->selected = 1;
    s->win2_x = 520; s->win2_y = 40;
    s->language = 1;
    s->done[0] = s->done[2] = 1;
    strcpy(s->code, "fn main() {\n    print(\"Hello from Finch\")\n}\n\n# This editor takes many lines.\n# Arrows, Home, End, PageUp and PageDown move,\n# Shift selects, Ctrl+C / X / V copy, cut and paste,\n# Ctrl+Z takes a change back, Ctrl+Y brings it again.\n# View > Word wrap folds a long line like this one to the width of the editor instead of scrolling sideways.");
    strcpy(s->name, "Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 g\xC4\x99\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84");
}

/* remembers the middle of the widget that was just made */
static void demo_at(float *x, float *y) {
    float ix, iy, iw, ih;
    lg_item_rect(&ix, &iy, &iw, &ih);
    *x = ix + (iw < 40 ? iw / 2 : 12);
    *y = iy + ih / 2;
}

/* the main menu bar along the top of the screen */
static void demo_menu_bar(demo_state *s) {
    static const char *recent[] = { "main.fch", "lexer.fch", "parser.fch" };
    int i;
    if (!lg_begin_menu_bar()) return;
    if (lg_begin_menu("File")) {
        if (lg_menu_item("New", "Ctrl+N", 0)) s->file_pick = 1;
        if (lg_menu_item("Open...", "Ctrl+O", 0)) s->file_pick = 2;
        if (lg_begin_menu("Recent")) {                  /* a menu inside a menu */
            for (i = 0; i < 3; i++) {
                if (lg_menu_item(recent[i], NULL, 0)) s->file_pick = 10 + i;
                if (i == 0) demo_at(&s->at_recent_x, &s->at_recent_y);
            }
            lg_end_menu();
        }
        lg_separator();
        if (lg_menu_item("Quit", "Ctrl+Q", 0)) s->quit = 1;
        lg_end_menu();
    }
    demo_at(&s->at_file_x, &s->at_file_y);
    if (lg_begin_menu("View")) {
        if (lg_menu_item("Light colours", NULL, s->light)) {
            s->light = !s->light;
            if (s->light) lg_style_light(); else lg_style_dark();
        }
        if (lg_menu_item("Word wrap", NULL, s->wrap)) s->wrap = !s->wrap;
        if (lg_menu_item("Line numbers", NULL, !s->no_numbers)) s->no_numbers = !s->no_numbers;
        lg_separator();
        if (lg_menu_item("Dock Style on the right", NULL, 0)) lg_dock_window("Style", NULL, LG_DOCK_RIGHT, 0.3f);
        if (lg_menu_item("Let Style float", NULL, 0)) lg_dock_window("Style", NULL, LG_DOCK_NONE, 0);
        /* the layout as text: a real program would keep it in a file */
        if (lg_menu_item("Remember the layout", NULL, 0)) lg_dock_save(s->layout, (int)sizeof s->layout);
        if (s->layout[0] && lg_menu_item("Bring the layout back", NULL, 0)) lg_dock_load(s->layout);
        lg_end_menu();
    }
    demo_at(&s->at_view_x, &s->at_file_y);
    lg_end_menu_bar();
}

static void demo_main(demo_state *s) {
    static const char *files[] = { "main.fch", "lexer.fch", "parser.fch", "gen.fch" };
    float x, y;
    int i;
    char line[64];
    lg_text("Loguia " LOGUIA_VERSION " - one C file, no dependencies");
    lg_text_wrapped("This paragraph wraps to the width of the window. Drag the corner of the "
                    "window to see it follow. Drag a title onto one of the squares to dock the window.");
    lg_separator();

    if (lg_button("Click me")) s->clicks++;
    demo_at(&s->at_button_x, &s->at_button_y);
    lg_same_line();
    lg_textf("clicked %d times", s->clicks);

    lg_checkbox("A checkbox", &s->check);
    demo_at(&s->at_check_x, &s->at_check_y);
    lg_slider("Volume", &s->volume, 0, 1);
    demo_at(&s->at_slider_x, &s->at_slider_y);
    lg_slider_int("Count", &s->count, 0, 10);              /* a click on the number types one */
    lg_item_rect(&s->at_count_x, &s->at_count_y, &s->at_count_w, &y);
    s->at_count_y += y / 2;
    if (lg_input_text("Name", s->name, (int)sizeof s->name) & LG_INPUT_ENTER) s->entered = 1;
    demo_at(&s->at_input_x, &s->at_input_y);
    lg_progress(s->volume, "progress follows Volume");

    if (lg_header("A tree", 1)) {
        s->tree_open = 0;
        if (lg_tree_begin("src", 0)) {
            s->tree_open = 1;
            for (i = 0; i < 4; i++) {
                lg_tree_begin(files[i], LG_TREE_LEAF | (s->selected == i ? LG_TREE_SELECTED : 0));
                if (lg_item_clicked()) s->selected = i;
            }
            lg_tree_end();
        }
        if (!s->tree_open) demo_at(&s->at_tree_x, &s->at_tree_y);
        if (lg_tree_begin("docs", 0)) {
            lg_text_colored(0x8B93A5FF, "(empty)");
            lg_tree_end();
        }
    }

    lg_text("A panel that scrolls:");
    lg_begin_panel("log", 0, 0, 0);
    for (i = 0; i < s->count * 10 + 10; i++) {
        snprintf(line, sizeof line, "line %d of the log", i + 1);
        lg_text(line);
        if (i == 0) {
            lg_item_rect(&x, &y, 0, 0);
            if (s->frames == 1) s->panel_first_y = y;
            s->panel_scrolled = y != s->panel_first_y;
            s->at_panel_x = x + 30; s->at_panel_y = y + 30;
        }
    }
    lg_end_panel();
}

static void demo_style(demo_state *s) {
    static const char *files[] = { "main.fch", "lexer.fch", "parser.fch", "gen.fch" };
    float x;
    int i;
    if (lg_button("Dark")) { lg_style_dark(); s->light = 0; }
    lg_item_rect(&x, &s->win2_now_y, 0, 0);
    s->win2_now_x = x;
    if (s->frames == 1) s->win2_first_x = x;
    s->win2_moved = x != s->win2_first_x;
    lg_same_line();
    if (lg_button("Light")) { lg_style_light(); s->light = 1; }
    lg_text("Every size can be changed:");
    lg_slider("padding", &lg_get_style()->padding_y, 0, 12);
    lg_slider("spacing", &lg_get_style()->spacing_y, 0, 16);
    lg_slider("indent", &lg_get_style()->indent, 0, 40);
    lg_slider("rounding", &lg_get_style()->rounding, 0, 12);
    lg_separator();
    lg_text("A list:");
    for (i = 0; i < 4; i++)
        if (lg_selectable(files[i], s->selected == i)) s->selected = i;
}

static void demo_more(demo_state *s) {
    static const char *files[] = { "main.fch", "lexer.fch", "parser.fch", "gen.fch" };
    static const char *tabs[] = { "Pickers", "Table", "Editor" };
    static const char *languages[] = { "C", "Finch", "Rust", "Zig", "Odin" };
    static const char *menu[] = { "nothing yet", "New file", "Open...", "Save" };
    static const int lines[] = { 233, 512, 1204, 871 };
    static const float widths[] = { 0, 90, 90 };
    int i, j, order[4];
    lg_tabs("tabs", tabs, 3, &s->tab);
    lg_item_rect(&s->at_tabs_x, &s->at_tabs_y, 0, 0);       /* the line under the tabs */
    if (s->tab == 0) {
        lg_combo("Language", &s->language, languages, 5);
        demo_at(&s->at_combo_x, &s->at_combo_y);
        if (lg_button("Menu")) lg_open_popup("menu");
        demo_at(&s->at_menu_x, &s->at_menu_y);
        lg_tooltip("Opens a popup menu.\nPopups close on a click outside or on Escape.");
        if (lg_begin_popup("menu")) {
            for (i = 1; i < 4; i++) {
                if (i == 3) lg_separator();
                if (lg_selectable(menu[i], 0)) s->menu_pick = i;
            }
            lg_end_popup();
        }
        lg_same_line();
        lg_textf("picked: %s", menu[s->menu_pick]);
        lg_text_wrapped("No mouse needed: Tab and Shift+Tab (or Up and Down) walk through the widgets, "
                        "Enter or Space presses, Left and Right move sliders.");
    } else if (s->tab == 1) {
        /* the lines between the columns can be dragged; a click on a heading sorts by that column */
        lg_begin_table("files", 3, widths, LG_TABLE_BORDERS | LG_TABLE_STRIPES | LG_TABLE_HEADER | LG_TABLE_RESIZE | LG_TABLE_SORT);
        for (i = 0; i < 4; i++) order[i] = i;
        if (lg_table_sort(&s->sort_column, &s->sort_down)) {
            for (i = 1; i < 4; i++)
                for (j = i; j > 0; j--) {
                    int a = order[j - 1], b = order[j], t;
                    int more = s->sort_column == 0 ? strcmp(files[a], files[b]) > 0 : s->sort_column == 1 ? lines[a] > lines[b] : s->done[a] > s->done[b];
                    int less = s->sort_column == 0 ? strcmp(files[a], files[b]) < 0 : s->sort_column == 1 ? lines[a] < lines[b] : s->done[a] < s->done[b];
                    if (s->sort_down ? !less : !more) break;
                    t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
                }
        } else s->sort_column = -1;
        s->first_row = order[0];
        lg_table_next(); lg_text("File");
        lg_item_rect(&s->at_head_x[0], &s->at_table_y, 0, 0);
        lg_table_next(); lg_text("Lines");
        lg_item_rect(&s->at_head_x[1], 0, 0, 0);
        lg_table_next(); lg_text("Done");
        lg_item_rect(&s->at_head_x[2], 0, 0, 0);
        for (i = 0; i < 4; i++) {
            lg_table_next(); lg_text(files[order[i]]);
            lg_table_next(); lg_textf("%d", lines[order[i]]);
            lg_table_next(); lg_checkbox("##done", &s->done[order[i]]);
        }
        lg_end_table();
        lg_text("Any widget can sit in a cell.");
    } else {
        lg_input_text_multiline("##code", s->code, (int)sizeof s->code, 0, 0,
                                (s->wrap ? LG_EDIT_WRAP : 0) | (s->no_numbers ? LG_EDIT_NO_LINE_NUMBERS : 0));
        demo_at(&s->at_editor_x, &s->at_editor_y);
    }
}

static void demo_ui(demo_state *s) {
    s->frames++;
    demo_menu_bar(s);

    /* LG_WINDOW_DOCK: the window can be dragged onto the squares that dock it.
       lg_begin returns 0 for a docked window that is behind another tab. */
    if (lg_begin("Loguia demo", 20, 40, 480, 620, LG_WINDOW_DOCK)) demo_main(s);
    lg_end();

    s->style_shown = lg_begin("Style", s->win2_x, s->win2_y, 300, 300, LG_WINDOW_DOCK);
    if (s->style_shown) demo_style(s);
    lg_end();

    s->more_shown = lg_begin("More", 520, 350, 560, 310, LG_WINDOW_DOCK);
    if (s->more_shown) demo_more(s);
    lg_end();
}
#endif
