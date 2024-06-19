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
    FILTER_ALL = 0,
    FILTER_IN_PROGRESS,
    FILTER_COMPLETED,
    FILTER_LOW,
    FILTER_MEDIUM,
    FILTER_HIGH,
} entry_filter_e;

static const char *entry_filters[] = {"ALL", "IN PROGRESS", "COMPLETED",
                                      "LOW", "MEDIUM",      "HIGH"};

typedef enum {
    DRAW_DIR_LEFT = 0,
    DRAW_DIR_RIGHT = 1,
} draw_direction_e;

typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM,
    PRIORITY_HIGH,
} entry_priority;

typedef struct {
    bool completed;
    char *desc;
    char *date;
    entry_priority priority;
} task_entry_t;

typedef struct {
    draw_direction_e draw_direction;
    entry_filter_e current_filter;
    task_entry_t tasks[1024];
    size_t num_tasks;
} app_state_t;

static app_state_t app_state = {.draw_direction = DRAW_DIR_LEFT,
                                .current_filter = 0};

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
    switch (app_state.draw_direction) {
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

        LfUIElementProps textProps = lf_get_theme().text_props;
        lf_push_style_props(textProps);
        btnProps.color = LF_NO_COLOR;

        lf_next_line();
        lf_set_ptr_x(div.aabb.size.x);
        app_state.draw_direction = DRAW_DIR_RIGHT;
        for (uint32_t i = 0; i < ARRAY_SIZE(entry_filters); i++) {
            LfUIElementProps btnProps = lf_get_theme().button_props;
            btnProps.border_width = 0;
            btnProps.corner_radius = 4;

            if (app_state.current_filter == (entry_filter_e)i) {

                btnProps.color = (LfColor){120, 120, 120, 255};
                btnProps.hover_color = LF_NO_COLOR;
            } else {

                btnProps.color = LF_NO_COLOR;
            }

            lf_push_style_props(btnProps);
            if (button(entry_filters[i]) == LF_CLICKED) {
                app_state.current_filter = i;
            }
            lf_pop_style_props();
        }
        app_state.draw_direction = DRAW_DIR_LEFT;
        lf_pop_style_props();
    }

    lf_div_end();
}

void app_add_task(app_state_t *app, const task_entry_t task) {
    if (app->num_tasks >= ARRAY_SIZE(app->tasks)) {
        printf("Error: above max tasks");
        return;
    }

    app->tasks[app->num_tasks] = task;
    app->num_tasks++;
}

int main() {
    app_add_task(&app_state, (task_entry_t){.desc = "Do the dishes",
                                            .date = "2024/02/02",
                                            .priority = PRIORITY_HIGH,
                                            .completed = false});

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
    theme.button_props.hover_color = (LfColor){50, 50, 50, 255};
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
