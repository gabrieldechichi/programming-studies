const sokol = @import("sokol");
const sapp = sokol.app;
const sg = sokol.gfx;
const sgapp = sokol.app_gfx_glue;
const slog = sokol.log;

const GlobalState = struct {
    pass_action: sg.PassAction,
};

export fn onInit() void {
    sg.setup(
        .{
            .context = sgapp.context(),
            .logger = .{ .func = slog.func },
        },
    );
    global_state.pass_action.colors[0] = .{
        .load_action = .CLEAR,
        .clear_value = .{
            .r = 1,
            .g = 1,
            .b = 0,
            .a = 1,
        },
    };
}

export fn onFrame() void {
    var c = &global_state.pass_action.colors[0].clear_value;
    c.g += 0.01;
    if (c.g >= 1) c.g -= 1;
    sg.beginDefaultPass(global_state.pass_action, screen_dimensions.width, screen_dimensions.height);
    sg.endPass();
    sg.commit();
}

export fn onCleanup() void {
    sg.shutdown();
}

export fn onEvent(event: ?*const sapp.Event) void {
    const e = event.?;
    if (e.type == sapp.EventType.KEY_DOWN and e.key_code == sapp.Keycode.ESCAPE) {
        sapp.requestQuit();
    }
}

var global_state: GlobalState = undefined;
const screen_dimensions = .{ .width = 640, .height = 480 };

pub fn main() void {
    sapp.run(.{
        .init_cb = onInit,
        .frame_cb = onFrame,
        .cleanup_cb = onCleanup,
        .event_cb = onEvent,
        .width = screen_dimensions.width,
        .height = screen_dimensions.height,
        .logger = .{ .func = slog.func },
    });
}
