const std = @import("std");
const sokol = @import("sokol");

const sg = sokol.gfx;
const sgapp = sokol.app_gfx_glue;
const slog = sokol.log;
const sapp = sokol.app;

fn printl(comptime fmt: []const u8, args: anytype) void {
    @import("std").debug.print(fmt ++ "\n", args);
}

const GlobalState = struct {
    pass_action: sg.PassAction,
};

var global: GlobalState = undefined;

export fn onInit() void {
    sokol.gfx.setup(.{ .context = sgapp.context(), .logger = .{ .func = slog.func } });
    global.pass_action.colors[0] = .{
        .load_action = .CLEAR,
        .clear_value = .{ .r = 1, .g = 1, .b = 0, .a = 1 },
    };
    printl("Initialize app. Backend: {}", .{sg.queryBackend()});
}

export fn onEvent(event: ?*const sapp.Event) void {
    const ev = event.?;
    switch (ev.type) {
        sapp.EventType.KEY_DOWN => {
            if (ev.key_code == sapp.Keycode.ESCAPE) {
                sapp.requestQuit();
            }
        },
        else => {},
    }
}

export fn onFrame() void {
    var c = &global.pass_action.colors[0].clear_value;
    c.g += 0.01;
    if (c.g >= 1) c.g -= 1;
    sg.beginDefaultPass(global.pass_action, sapp.width(), sapp.height());
    sg.endPass();
    sg.commit();
}

export fn cleanup() void {
    sg.shutdown();
}

pub fn main() !void {
    sapp.run(.{
        .init_cb = onInit,
        .event_cb = onEvent,
        .frame_cb = onFrame,
        .cleanup_cb = cleanup,
        .width = 640,
        .height = 480,
        .icon = .{ .sokol_default = true },
        .window_title = "clear",
        .logger = .{ .func = slog.func },
        .win32_console_attach = true,
    });
}

test "simple test" {
    var list = std.ArrayList(i32).init(std.testing.allocator);
    defer list.deinit(); // try commenting this out and see if zig detects the memory leak!
    try list.append(42);
    try std.testing.expectEqual(@as(i32, 42), list.pop());
}
