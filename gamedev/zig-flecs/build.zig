const std = @import("std");
const zflecs = @import("./vendor/zig-gamedev/libs/zflecs/build.zig");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "zig-flecs",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    b.installArtifact(exe);
    buildZFlecs(b, exe, target, optimize);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}

const vendorLibs = "./vendor/zig-gamedev/libs/";

fn buildZFlecs(b: *std.Build, exe: *std.build.Step.Compile, target: anytype, optimize: std.builtin.OptimizeMode) void {
    const zflecsPath = vendorLibs ++ "zflecs/";
    const m = b.addModule("zflecs", .{
        .source_file = .{ .path = zflecsPath ++ "src/zflecs.zig" },
    });
    exe.addModule("zflecs", m);

    const flecs = b.addStaticLibrary(.{
        .name = "flecs",
        .target = target,
        .optimize = optimize,
    });
    flecs.linkLibC();
    flecs.addIncludePath(.{ .path = zflecsPath ++ "libs/flecs" });
    flecs.addCSourceFile(.{
        .file = .{ .path = zflecsPath ++ "libs/flecs/flecs.c" },
        .flags = &.{
            "-fno-sanitize=undefined",
            "-DFLECS_NO_CPP",
            "-DFLECS_USE_OS_ALLOC",
            if (@import("builtin").mode == .Debug) "-DFLECS_SANITIZE" else "",
        },
    });
    b.installArtifact(flecs);

    if (target.getOsTag() == .windows) {
        flecs.linkSystemLibrary("ws2_32");
    }

    exe.linkLibrary(flecs);

    // const test_step = b.step("test", "Run zflecs tests");
    //
    // const tests = b.addTest(.{
    //     .name = "zflecs-tests",
    //     .root_source_file = .{ .path = "src/zflecs.zig" },
    //     .target = target,
    //     .optimize = optimize,
    // });
    // b.installArtifact(tests);
    //
    // tests.linkLibrary(flecs);
    //
    // test_step.dependOn(&b.addRunArtifact(tests).step);
    // return m;
}
