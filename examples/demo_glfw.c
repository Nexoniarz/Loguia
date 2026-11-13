/* The Loguia demo in a real window, using the GLFW and OpenGL modules.
 *
 *   Linux / BSD:  cc demo_glfw.c -o demo -lglfw -lGL
 *   macOS:        cc demo_glfw.c -o demo -lglfw -framework OpenGL
 *   Windows:      cc demo_glfw.c -o demo.exe -lglfw3 -lopengl32 -lgdi32
 *
 *   ./demo                 opens the window
 *   ./demo shot.ppm        draws a few frames in a hidden window, saves a picture and quits
 */
#define GL_SILENCE_DEPRECATION          /* macOS: yes, we know OpenGL is old */
#include <GLFW/glfw3.h>

#define LOGUIA_IMPLEMENTATION
#define LOGUIA_GL
#define LOGUIA_GLFW
#include "../loguia.h"
#include "demo_ui.h"

#include <stdlib.h>

int main(int argc, char **argv) {
    GLFWwindow *window;
    demo_state st;
    int fw, fh, frames = 0;

    if (!glfwInit()) { fprintf(stderr, "could not start GLFW\n"); return 1; }
    if (argc > 1) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(1100, 680, "Loguia demo", NULL, NULL);
    if (!window) { fprintf(stderr, "could not open a window\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    lg_init();
    lg_gl_init();
    lg_glfw_init(window);
    demo_init(&st);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        lg_glfw_begin_frame();
        demo_ui(&st);
        lg_end_frame();

        glfwGetFramebufferSize(window, &fw, &fh);
        glViewport(0, 0, fw, fh);
        if (st.light) glClearColor(0.80f, 0.82f, 0.86f, 1); else glClearColor(0.055f, 0.065f, 0.085f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        lg_gl_render(fw, fh);

        if (argc > 1 && ++frames == 3) {
            unsigned char *px = malloc((size_t)fw * fh * 3);
            FILE *f = fopen(argv[1], "wb");
            int y;
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, fw, fh, GL_RGB, GL_UNSIGNED_BYTE, px);
            if (!f) { perror(argv[1]); return 1; }
            fprintf(f, "P6\n%d %d\n255\n", fw, fh);
            for (y = fh - 1; y >= 0; y--) fwrite(px + (size_t)y * fw * 3, 1, (size_t)fw * 3, f);
            fclose(f);
            free(px);
            break;
        }
        glfwSwapBuffers(window);
        if (st.quit) break;                 /* File > Quit */
    }
    lg_gl_shutdown();
    lg_shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
