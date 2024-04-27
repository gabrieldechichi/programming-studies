const std = @import("std");

const Project = struct { name: []const u8, path: []const u8 };

pub fn build(b: *std.Build) void {

    // const opt_use_gl = b.option(bool, "gl", "Force OpenGL (default: false)") orelse false;
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const projects = [_]Project{
        .{ .name = "main", .path = "src/main.zig" },
        .{ .name = "opengl-1-helloworld", .path = "./src/learnopengl/1_hello.zig" },
        .{ .name = "opengl-2-hellotriangle", .path = "./src/learnopengl/2_hello_triangle/2_hello_triangle.zig" },
    };

    inline for (projects) |p| {
        const exe = b.addExecutable(.{
            .name = p.name,
            .root_source_file = .{ .path = p.path },
            .target = target,
            .optimize = optimize,
        });
        b.installArtifact(exe);

        //sokol
        {
            const dep_sokol = b.dependency("sokol", .{
                .target = target,
                .optimize = optimize,
                .gl = true
            });

            exe.root_module.addImport("sokol", dep_sokol.module("sokol"));
        }

        //run
        {
            const run_cmd = b.addRunArtifact(exe);

            run_cmd.step.dependOn(b.getInstallStep());

            if (b.args) |args| {
                run_cmd.addArgs(args);
            }

            const run_step = b.step(exe.name, "Run " ++ p.name);
            run_step.dependOn(&run_cmd.step);
        }
    }

    //tests
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
