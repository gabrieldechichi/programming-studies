const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .cpu_arch = .wasm32, .os_tag = .emscripten } });

    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSmall });

    const lib = b.addSharedLibrary(.{
        .name = "zig_wasm_3_raylib",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    const rl = @import("raylib-zig/build.zig");
    const emcc = @import("raylib-zig/emcc.zig");

    var raylib = rl.getModule(b, "raylib-zig");

    const exe_lib = emcc.compileForEmscripten(b, lib.name, lib.root_src.?.path, target, optimize);
    exe_lib.addModule("raylib", raylib);
    const raylib_lib = rl.getRaylib(b, target, optimize);

    // Note that raylib itself isn't actually added to the exe_lib
    // output file, so it also needs to be linked with emscripten.
    exe_lib.linkLibrary(raylib_lib);
    const link_step = try emcc.linkWithEmscripten(b, &[_]*std.Build.Step.Compile{ exe_lib, raylib_lib });
    link_step.addArg("--embed-file");
    link_step.addArg("resources/");

    const run_step = try emcc.emscriptenRunStep(b);
    run_step.step.dependOn(&link_step.step);
    const run_option = b.step(lib.name, "description");
    run_option.dependOn(&run_step.step);

    b.installArtifact(lib);
}
