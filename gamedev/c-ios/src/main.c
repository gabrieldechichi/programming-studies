#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_gl.h"

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    
    // Give Metal time to initialize properly
    sgl_setup(&(sgl_desc_t){
        .logger.func = slog_func,
    });
}

void frame(void) {
    // setup viewport
    int w = sapp_width();
    int h = sapp_height();
    
    // begin frame
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, +1.0f);
    
    // draw a colorful triangle
    sgl_begin_triangles();
    
    sgl_c3f(1.0f, 0.0f, 0.0f);  // red
    sgl_v2f(w*0.5f, h*0.25f);   // top
    
    sgl_c3f(0.0f, 1.0f, 0.0f);  // green  
    sgl_v2f(w*0.25f, h*0.75f);  // bottom left
    
    sgl_c3f(0.0f, 0.0f, 1.0f);  // blue
    sgl_v2f(w*0.75f, h*0.75f);  // bottom right
    
    sgl_end();
    
    // render sokol_gl
    sg_begin_pass(&(sg_pass){
        .action = { 
            .colors[0] = { .load_action=SG_LOADACTION_CLEAR, .clear_value={1.0f, 0.0f, 0.0f, 1.0f } }
        },
        .swapchain = sglue_swapchain()
    });
    sgl_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sgl_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .fullscreen = true,
        .high_dpi = true,
        .window_title = "Triangle (sokol-app)",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}