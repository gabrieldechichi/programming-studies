#include "GLFW/glfw3.h"
#include "leif/leif.h"
#include <GL/gl.h>
#include <cglm/types-struct.h>
#include <stdio.h>

#define WIDTH 1280
#define HEIGHT 720
#define WINMARGIN 2

#define concat(a, b) a##b
#define macro_var(name) concat(name, __LINE__)
#define defer(start, end)                                                      \
    for (int macro_var(_i_) = ((start), 0); !macro_var(_i_);                     \
         (macro_var(_i_) += 1), (end))

#define lf_scope defer(lf_begin(), lf_end())
#define lf_div_scope(start_args) defer(lf_div_begin start_args, lf_div_end)

int main() {
    glfwInit();
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Todo", NULL, NULL);
    glfwMakeContextCurrent(window);

    lf_init_glfw(WIDTH, HEIGHT, window);

    LfTheme theme = lf_get_theme();
    theme.div_props.color = LF_NO_COLOR;
    lf_set_theme(theme);

    LfFont titlefont = lf_load_font("./fonts/inter.ttf", 40);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        lf_scope {
            lf_div_scope(
                (((vec2s){WINMARGIN, WINMARGIN}),
                 ((vec2s){WIDTH - WINMARGIN * 2.0f, HEIGHT - WINMARGIN * 2}),
                 true)) {
                lf_push_font(&titlefont);
                lf_text("Your TODOS");
                lf_pop_font();
            }
        }

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    lf_terminate();
    lf_free_font(&titlefont);
    glfwDestroyWindow(window);
    glfwTerminate();
}
