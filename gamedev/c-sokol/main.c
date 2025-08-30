//------------------------------------------------------------------------------
//  Simple sokol_app.h window example
//------------------------------------------------------------------------------
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"

static sg_pass_action pass_action;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    
    // Set up clear color (blue background)
    pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.2f, 0.4f, 0.8f, 1.0f }
        }
    };
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ 
        .action = pass_action, 
        .swapchain = sglue_swapchain() 
    });
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "Sokol Window",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
