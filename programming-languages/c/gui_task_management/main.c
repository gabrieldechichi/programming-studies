#include "GLFW/glfw3.h"
#include "leif/leif.h"
#include <GL/gl.h>
#include <cglm/types-struct.h>
#include <stdio.h>

#define WIDTH 1280
#define HEIGHT 720
#define WINMARGIN 2

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

        lf_begin();
        lf_div_begin(
            ((vec2s){WINMARGIN, WINMARGIN}),
            ((vec2s){WIDTH - WINMARGIN * 2.0f, HEIGHT - WINMARGIN * 2}), true);

        lf_push_font(&titlefont);
        lf_text("Your TODOS");
        lf_pop_font();

        lf_div_end();
        lf_end();

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    lf_terminate();
    glfwDestroyWindow(window);
    glfwTerminate();
}
