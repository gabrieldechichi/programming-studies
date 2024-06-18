#include "GLFW/glfw3.h"
#include "leif/leif.h"
#include <GL/gl.h>
#include <cglm/types-struct.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

#define WIDTH 1280
#define HEIGHT 720
#define WINMARGIN 8
#define DIV_DEFAULT_PAD 8

#define set_margin(p, v)                                                       \
    p.margin_left = v;                                                         \
    p.margin_right = v;                                                        \
    p.margin_top = v;                                                          \
    p.margin_bottom = v

typedef enum {
    DRAW_DIR_LEFT = 0,
    DRAW_DIR_RIGHT = 1,
} draw_direction_e;

static draw_direction_e draw_direction = DRAW_DIR_LEFT;

LfDiv div_begin(vec2s pos, vec2s size, bool scrollable) {
    lf_div_begin(pos, size, scrollable);
    return lf_get_current_div();
}

LfDiv div_begin_color(vec2s pos, vec2s size, bool scrollable, LfColor color) {
    LfUIElementProps props = lf_get_theme().div_props;
    props.color = color;
    lf_push_style_props(props);
    lf_div_begin(pos, size, scrollable);
    lf_pop_style_props();
    return lf_get_current_div();
}

LfClickableItemState button(const char *text) {
    switch (draw_direction) {
    case DRAW_DIR_RIGHT: {
        float x = lf_get_ptr_x();
        lf_set_no_render(true);
        lf_set_line_should_overflow(false);
        lf_button(text);
        lf_set_line_should_overflow(true);
        lf_set_no_render(false);

        LfUIElementProps btn_props = lf_get_theme().button_props;
        float width = lf_get_ptr_x() - x + btn_props.margin_right;
        lf_set_ptr_x(x - width);
        LfClickableItemState btn = lf_button(text);
        lf_set_ptr_x(x - width);
        return btn;
    }
    case DRAW_DIR_LEFT:
    default:
        return lf_button(text);
    }
}

void draw_top_bar(LfFont titlefont) {
    LfDiv div = div_begin((vec2s){0}, (vec2s){{WIDTH, HEIGHT}}, true);

    LfUIElementProps divProps = lf_get_theme().div_props;

    lf_push_font(&titlefont);
    lf_text("Your Todos");
    lf_pop_font();

    LfUIElementProps btnProps = lf_get_theme().button_props;
    btnProps.color = (LfColor){65, 167, 204, 255};
    btnProps.padding = 15;
    btnProps.border_width = 0.0f;
    btnProps.corner_radius = 4.0f;
    lf_push_style_props(btnProps);

    float btnwidth = 120;
    lf_set_ptr_x(div.aabb.size.x - divProps.padding - btnwidth -
                 2.0f * btnProps.padding - btnProps.margin_right);
    lf_button_fixed("Add Todo", btnwidth, -1);
    lf_pop_style_props();

    // filters
    {
        static const char *filters[] = {"ALL", "IN PROGRESS", "COMPLETED",
                                        "LOW", "MEDIUM",      "HIGH"};

        LfUIElementProps textProps = lf_get_theme().text_props;
        lf_push_style_props(textProps);
        LfUIElementProps btnProps = lf_get_theme().button_props;
        btnProps.border_width = 0;
        btnProps.corner_radius = 4;
        btnProps.color = LF_NO_COLOR;
        lf_push_style_props(btnProps);

        lf_next_line();
        lf_set_ptr_x(div.aabb.size.x);
        draw_direction = DRAW_DIR_RIGHT;
        for (uint32_t i = 0; i < ARRAY_SIZE(filters); i++) {
            button(filters[i]);
        }
        draw_direction = DRAW_DIR_LEFT;
        lf_pop_style_props();
        lf_pop_style_props();
    }

    lf_div_end();
}

int main() {
    glfwInit();
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Todo", NULL, NULL);
    glfwMakeContextCurrent(window);

    lf_init_glfw(WIDTH, HEIGHT, window);

    LfTheme theme = lf_get_theme();
    theme.div_props.color = LF_NO_COLOR;
    theme.div_props.padding = DIV_DEFAULT_PAD;

    set_margin(theme.text_props, DIV_DEFAULT_PAD);
    set_margin(theme.button_props, DIV_DEFAULT_PAD);
    theme.text_props.color = LF_WHITE;
    theme.button_props.text_color = LF_WHITE;
    theme.button_props.hover_color = (LfColor) { 50, 50, 50, 255};
    lf_set_theme(theme);

    LfFont titlefont = lf_load_font("./fonts/inter.ttf", 40);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);

        lf_begin();

        // root div

        draw_top_bar(titlefont);
        lf_div_end();
        lf_end();

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    lf_free_font(&titlefont);
    lf_terminate();
    glfwDestroyWindow(window);
    glfwTerminate();
}
