const std = @import("std");
const raySdk = @import("raylib/src/build.zig");
const emcc = @import("emcc.zig");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    if (target.cpu_arch != null and target.cpu_arch.?.isWasm()) {
        try buildWeb(b, target, optimize);
    } else {
        try buildNative(b, target, optimize);
    }
}

fn buildWeb(b: *std.Build, target: anytype, optimize: std.builtin.OptimizeMode) !void {
    try emcc.resolveEmscriptenSysRoot(b);

    //compile targets
    var exe_lib = try emcc.compileForEmscripten(b, "game", "src/main.zig", target, optimize);
    exe_lib.addIncludePath(.{ .path = "raylib/src" });
    var raylib_lib = try raySdk.addRaylib(b, target, optimize, .{});
    exe_lib.linkLibrary(raylib_lib);

    //linking
    var link_step = try emcc.linkForEmscripten(b, &[_]*std.build.CompileStep{ exe_lib, raylib_lib }, optimize);

    link_step.addArg("--embed-file");
    link_step.addArg("resources/");

    //build step
    const build_step = b.step("build", "Build game");
    build_step.dependOn(&link_step.step);

    //run step
    const run_step = b.step("run", "Run game");
    const emscripten_run_step = try emcc.emscriptenRunStep(b);
    emscripten_run_step.step.dependOn(&link_step.step);
    run_step.dependOn(&emscripten_run_step.step);
}

fn buildNative(b: *std.Build, target: anytype, optimize: std.builtin.OptimizeMode) !void {
    const exe = b.addExecutable(.{
        .name = "zig_wasm_4_raylib",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    b.installArtifact(exe);

    var raylib = try raySdk.addRaylib(b, target, optimize, .{});
    exe.addIncludePath(.{ .path = "raylib/src" });
    exe.linkLibrary(raylib);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
