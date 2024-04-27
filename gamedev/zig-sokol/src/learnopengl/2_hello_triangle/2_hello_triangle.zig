const sokol = @import("sokol");
const sapp = sokol.app;
const sg = sokol.gfx;
const sgapp = sokol.app_gfx_glue;
const slog = sokol.log;
const std = @import("std");

const GlobalState = struct {
    bind: sg.Bindings,
    pipe: sg.Pipeline,
    allocator: std.mem.Allocator,
};

fn srcDir() []const u8 {
    comptime {
        const source_path = @src().file;
        var split = std.mem.splitBackwardsAny(u8, source_path, std.fs.path.sep_str);
        _ = split.next();
        return split.rest();
    }
}

fn loadShader(file_name: []const u8) ![]const u8 {
    const cwd = std.fs.cwd();
    const dir = comptime srcDir();
    const file_path = try std.fs.path.join(global_state.allocator, &[2][]const u8{ dir, file_name });
    defer global_state.allocator.free(file_path);

    const code = try cwd.readFileAlloc(global_state.allocator, file_path, 10000000);
    defer global_state.allocator.free(code);

    return try global_state.allocator.dupeZ(u8, code);
}

export fn onInit() void {
    global_state.allocator = std.heap.page_allocator;

    sg.setup(
        .{
            .context = sgapp.context(),
            .logger = .{ .func = slog.func },
        },
    );

    global_state.bind.vertex_buffers[0] = sg.makeBuffer(.{
        .data = sg.asRange(
            &[_]f32{
                // positions         colors
                0.0,  0.5,  0.5, 1.0, 0.0, 0.0, 1.0,
                0.5,  -0.5, 0.5, 0.0, 1.0, 0.0, 1.0,
                -0.5, -0.5, 0.5, 0.0, 0.0, 1.0, 1.0,
            },
        ),
    });

    const shader_desc = shaderDesc();
    const shader = sg.makeShader(shader_desc);
    var pipe_desc: sg.PipelineDesc = .{ .shader = shader };
    pipe_desc.layout.attrs[0].format = .FLOAT3;
    pipe_desc.layout.attrs[1].format = .FLOAT4;

    global_state.pipe = sg.makePipeline(pipe_desc);
}

export fn onFrame() void {
    sg.beginDefaultPass(.{}, sapp.width(), sapp.height());
    sg.applyPipeline(global_state.pipe);
    sg.applyBindings(global_state.bind);
    sg.draw(0, 3, 1);
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

pub fn main() void {
    sapp.run(.{
        .init_cb = onInit,
        .frame_cb = onFrame,
        .cleanup_cb = onCleanup,
        .event_cb = onEvent,
        .width = 640,
        .height = 480,
        .gl_major_version = 3,
        .gl_minor_version = 3,
        .logger = .{ .func = slog.func },
    });
}

fn shaderDesc() sg.ShaderDesc {
    var desc: sg.ShaderDesc = .{};
    desc.attrs[0].name = "position";
    desc.attrs[1].name = "color0";
    const vs = loadShader("./vert.glsl") catch "";
    const fs = loadShader("./frag.glsl") catch "";
    desc.vs.source = vs.ptr;
    desc.fs.source = fs.ptr;
    return desc;
}
