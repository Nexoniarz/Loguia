/* The Loguia demo in a real window with no library at all: LOGUIA_WINDOW opens the window,
 * reads the mouse and keyboard and draws, using only what the system itself provides.
 *
 *   Linux / BSD:  cc demo_window.c -o demo_window -lX11
 *     on Wayland: cc -DLOGUIA_WAYLAND demo_window.c -o demo_window -lwayland-client -lxkbcommon -lX11
 *                 (a real Wayland window; X11 stays as the fallback. Add -DLOGUIA_NO_X11 to drop it and -lX11.)
 *   Windows:      cc demo_window.c -o demo_window.exe -lgdi32
 *   macOS:        cc demo_window.c -o demo_window -framework Cocoa
 */
#define LOGUIA_IMPLEMENTATION
#define LOGUIA_WINDOW
#include "../loguia.h"
#include "demo_ui.h"

int main(void) {
    demo_state st;
    if (!lg_window_open("Loguia demo", 1100, 680)) { fprintf(stderr, "could not open a window\n"); return 1; }
    demo_init(&st);
    while (lg_window_begin_frame()) {
        demo_ui(&st);
        lg_window_end_frame(st.light ? 0xCCD1DBFF : 0x0E1116FF);
        if (st.quit) break;                 /* File > Quit */
    }
    lg_window_close();
    return 0;
}
