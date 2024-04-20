const std = @import("std");
const raySdk = @import("raylib/src/build.zig");

const builtin = @import("builtin");
const emccOutputDir = "zig-out" ++ std.fs.path.sep_str ++ "htmlout" ++ std.fs.path.sep_str;
const emccOutputFile = "index.html";

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
    //START HACKS
    // HACK: issue: https://github.com/ziglang/zig/issues/16776 is where the issue is submitted.
    // Zig building to emscripten doesn't work, because the Zig standard library
    // is missing some things in the C system. "std/c.zig" is missing fd_t,
    // which causes compilation to fail. So build to wasi instead, until it gets
    // fixed.
    const new_target = std.zig.CrossTarget {
        .cpu_arch = target.cpu_arch,
        .cpu_model = target.cpu_model,
        .cpu_features_add = target.cpu_features_add,
        .cpu_features_sub = target.cpu_features_sub,
        .os_tag = .wasi,
        .os_version_min = target.os_version_min,
        .os_version_max = target.os_version_max,
        .glibc_version = target.glibc_version,
        .abi = target.abi,
        .dynamic_linker = target.dynamic_linker,
        .ofmt = target.ofmt,
    };

    //HACK: Related to 16776. when the updateTargetForWeb workaround gets removed, see if those are nessesary anymore
    const webhack_c =
        \\// Zig adds '__stack_chk_guard', '__stack_chk_fail', and 'errno',
        \\// which emscripten doesn't actually support.
        \\// Seems that zig ignores disabling stack checking,
        \\// and I honestly don't know why emscripten doesn't have errno.
        \\#include <stdint.h>
        \\uintptr_t __stack_chk_guard;
        \\//I'm not certain if this means buffer overflows won't be detected,
        \\// However, zig is pretty safe from those, so don't worry about it too much.
        \\void __stack_chk_fail(void){}
        \\int errno;
    ;

    const webhack_c_file_step = b.addWriteFiles();
    const web_hack_c_file = webhack_c_file_step.add("webhack.c", webhack_c);
    //END HACKS

    //try resolve emsdk path if not set
    if (b.sysroot == null) {
        const emsdk_path = try std.process.getEnvVarOwned(b.allocator, "EMSDK");
        b.sysroot = try std.fs.path.join(b.allocator, &.{ emsdk_path, "/upstream/emscripten" });
    }

    // const emcc_run_arg = try std.fs.path.join(b.allocator, &.{ b.sysroot.?, "emcc.bat" });
    const emcc_run_arg = try getEmccCmd(b);
    defer b.allocator.free(emcc_run_arg);

    var raylib_lib = try raySdk.addRaylib(b, target, optimize, .{});

    const exe_lib = b.addStaticLibrary(.{
        .name = "game",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = new_target,
        .optimize = optimize,
    });
    exe_lib.addIncludePath(.{ .path = "raylib/src" });
    exe_lib.addCSourceFile(.{ .file = web_hack_c_file, .flags = &[_][]u8{} });
    exe_lib.step.dependOn(&webhack_c_file_step.step);

    exe_lib.linkLibrary(raylib_lib);

    b.installArtifact(exe_lib);
    b.installArtifact(raylib_lib);

    var cwd = std.fs.cwd();
    try cwd.makePath("./zig-out/htmlout");

    const link_step = b.addSystemCommand(&[_][]const u8{emcc_run_arg});
    link_step.addFileArg(exe_lib.getEmittedBin());
    link_step.step.dependOn(&exe_lib.step);
    link_step.addFileArg(raylib_lib.getEmittedBin());
    link_step.step.dependOn(&raylib_lib.step);

    link_step.addArgs(&[_][]const u8{
        "-o",
        emccOutputDir ++ emccOutputFile,
        "-sFULL-ES3=1",
        "-sUSE_GLFW=3",
        "-sASYNCIFY",
        "-O0",
        "--emrun",
    });

    // const run_step = try emscriptenRunStep(b);
    // run_step.step.dependOn(&emcc_command.step);
    const run_option = b.step("game", "Game");
    run_option.dependOn(&link_step.step);
}

pub fn getEmrunCmd(b: *std.Build) ![]u8 {
    if (b.sysroot == null) {
        const emsdk_path = try std.process.getEnvVarOwned(b.allocator, "EMSDK");
        b.sysroot = try std.fs.path.join(b.allocator, &.{ emsdk_path, "/upstream/emscripten" });
    }
    if (b.sysroot == null) {
        @panic("Pass '--sysroot \"[path to emsdk installation]/upstream/emscripten\"'");
    }
    // If compiling on windows , use emrun.bat.
    const emrunExe = switch (builtin.os.tag) {
        .windows => "emrun.bat",
        else => "emrun",
    };

    return try std.fs.path.join(b.allocator, &.{ b.sysroot.?, emrunExe });
}

pub fn getEmccCmd(b: *std.Build) ![]u8 {
    if (b.sysroot == null) {
        const emsdk_path = try std.process.getEnvVarOwned(b.allocator, "EMSDK");
        b.sysroot = try std.fs.path.join(b.allocator, &.{ emsdk_path, "/upstream/emscripten" });
    }
    if (b.sysroot == null) {
        @panic("Pass '--sysroot \"[path to emsdk installation]/upstream/emscripten\"'");
    }
    // If compiling on windows , use emrun.bat.
    const emrunExe = switch (builtin.os.tag) {
        .windows => "emcc.bat",
        else => "emcc",
    };

    return try std.fs.path.join(b.allocator, &.{ b.sysroot.?, emrunExe });
}

pub fn emscriptenRunStep(b: *std.Build) !*std.Build.Step.Run {
    const emcc_run_arg = try getEmrunCmd(b);
    defer b.allocator.free(emcc_run_arg);

    const run_cmd = b.addSystemCommand(&[_][]const u8{ emcc_run_arg, emccOutputDir ++ emccOutputFile });
    return run_cmd;
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
