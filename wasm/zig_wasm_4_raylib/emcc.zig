const std = @import("std");
const builtin = @import("builtin");
const emccOutputDir = "zig-out" ++ std.fs.path.sep_str ++ "htmlout" ++ std.fs.path.sep_str;
const emccOutputFile = "index.html";

pub fn compileForEmscripten(b: *std.Build, name: []const u8, root_source_file: []const u8, target: anytype, optimize: std.builtin.OptimizeMode) !*std.build.Step.Compile {
    //START HACKS
    // HACK: issue: https://github.com/ziglang/zig/issues/16776 is where the issue is submitted.
    // Zig building to emscripten doesn't work, because the Zig standard library
    // is missing some things in the C system. "std/c.zig" is missing fd_t,
    // which causes compilation to fail. So build to wasi instead, until it gets
    // fixed.
    const new_target = std.zig.CrossTarget{
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

    const exe_lib = b.addStaticLibrary(.{
        .name = name,
        .root_source_file = .{ .path = root_source_file },
        .target = new_target,
        .optimize = optimize,
    });
    exe_lib.addCSourceFile(.{ .file = web_hack_c_file, .flags = &[_][]u8{} });
    exe_lib.step.dependOn(&webhack_c_file_step.step);
    return exe_lib;
}

pub fn linkForEmscripten(b: *std.Build, itemsToLink: []const *std.Build.Step.Compile, optimize: std.builtin.OptimizeMode) !*std.build.Step.Run {
    const emcc_run_arg = try getEmscriptenCmd(b, "emcc");
    defer b.allocator.free(emcc_run_arg);

    const link_step = b.addSystemCommand(&[_][]const u8{emcc_run_arg});

    for (itemsToLink) |item| {
        b.installArtifact(item);
        link_step.addFileArg(item.getEmittedBin());
        link_step.step.dependOn(&item.step);
    }

    var cwd = std.fs.cwd();
    try cwd.makePath(emccOutputDir);

    const opt_flag = switch (optimize) {
        .Debug => "-O0",
        .ReleaseSafe => "-O0",
        .ReleaseSmall => "-Oz",
        .ReleaseFast => "-O3",
    };

    link_step.addArgs(&[_][]const u8{
        "-o",
        emccOutputDir ++ emccOutputFile,
        "-sFULL-ES3=1",
        "-sUSE_GLFW=3",
        "-sASYNCIFY",
        opt_flag,
        "--shell-file",
        "www/minshell.html",
        "--emrun",
    });
    return link_step;
}

pub fn resolveEmscriptenSysRoot(b: *std.Build) !void {
    if (b.sysroot == null) {
        const emsdk_path = try std.process.getEnvVarOwned(b.allocator, "EMSDK");
        b.sysroot = try std.fs.path.join(b.allocator, &.{ emsdk_path, "/upstream/emscripten" });
    }
}

pub fn getEmscriptenCmd(b: *std.Build, cmd: []const u8) ![]u8 {
    if (b.sysroot == null) {
        @panic("Pass '--sysroot \"[path to emsdk installation]/upstream/emscripten\"'");
    }

    if (builtin.os.tag == .windows) {
        const cmd_bat = try std.fmt.allocPrint(b.allocator, "{s}.bat", .{cmd});
        defer b.allocator.free(cmd_bat);
        return try std.fs.path.join(b.allocator, &.{ b.sysroot.?, cmd_bat });
    }
    return try std.fs.path.join(b.allocator, &.{ b.sysroot.?, cmd });
}

pub fn emscriptenRunStep(b: *std.Build) !*std.Build.Step.Run {
    const emcc_run_arg = try getEmscriptenCmd(b, "emrun");
    defer b.allocator.free(emcc_run_arg);

    const run_cmd = b.addSystemCommand(&[_][]const u8{ emcc_run_arg, emccOutputDir ++ emccOutputFile });
    return run_cmd;
}
