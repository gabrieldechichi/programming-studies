const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .cpu_arch = .wasm32, .os_tag = .freestanding } });

    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSmall });

    const lib = b.addSharedLibrary(.{
        .name = "zig_wasm_2",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    lib.export_symbol_names = &.{ "alloc", "free", "add", "sub", "zlog" };
    b.installArtifact(lib);
}
