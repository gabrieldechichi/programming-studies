const std = @import("std");

// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "asteroids",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    //link raylib
    {
        const rl = @import("raylib-zig/build.zig");
        var raylib = rl.getModule(b, "raylib-zig");
        var raylib_math = rl.math.getModule(b, "raylib-zig");
        rl.link(b, exe, target, optimize);
        exe.addModule("raylib", raylib);
        exe.addModule("raylib-math", raylib_math);
    }

    b.installArtifact(exe);

    //run cmd
    {
        const run_cmd = b.addRunArtifact(exe);
        run_cmd.step.dependOn(b.getInstallStep());

        // This allows the user to pass arguments to the application in the build
        // command itself, like this: `zig build run -- arg1 arg2 etc`
        // if (b.args) |args| {
        //     run_cmd.addArgs(args);
        // }

        const run_step = b.step("run", "Run the app");
        run_step.dependOn(&run_cmd.step);
    }

    //test cmd
    {
        const unit_tests = b.addTest(.{
            .root_source_file = .{ .path = "src/main.zig" },
            .target = target,
            .optimize = optimize,
        });
        const run_unit_tests = b.addRunArtifact(unit_tests);
        const test_step = b.step("test", "Run unit tests");
        test_step.dependOn(&run_unit_tests.step);
    }
}
