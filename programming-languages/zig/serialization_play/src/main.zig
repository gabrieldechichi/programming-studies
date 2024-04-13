const std = @import("std");
const assert = std.debug.assert;
const debugPrint = std.debug.print;
const Allocator = std.mem.Allocator;
const Array = std.ArrayList;

fn toJson(comptime T: type, value: T, allocator: Allocator) !std.json.Value {
    switch (@typeInfo(T)) {
        .Bool => return std.json.Value{ .bool = value },
        .Int => return std.json.Value{ .integer = value },
        .Array => {
            var arr = Array(std.json.Value).init(allocator);
            for (value) |element| {
                const v = try toJson(@TypeOf(element), element, allocator);
                try arr.append(v);
            }
            return std.json.Value{.array = arr};
        },
        .Struct => |struct_info| {
            const isArray: bool = comptime blk: {
                const name = @typeName(T);
                break :blk std.mem.indexOf(u8, name, "array_list.ArrayListAligned(") != null;
            };
            if (isArray) {
                var arr = Array(std.json.Value).init(allocator);
                for (value.items) |element| {
                    const v = try toJson(@TypeOf(element), element, allocator);
                    try arr.append(v);
                }
                return std.json.Value{ .array = arr };
            } else {
                var object = std.json.ObjectMap.init(allocator);
                inline for (struct_info.fields) |field| {
                    const v = try toJson(field.type, @field(value, field.name), allocator);
                    try object.put(field.name, v);
                }
                return std.json.Value{ .object = object };
            }
        },
        else => {
            @compileError("Unsupported type " ++ @typeName(T));
        },
    }
}

pub fn main() !void {
    const Pet = struct {
        id: i32,
        number: bool,
        arr: Array(u8),
        arr2: [3]u8,
    };
    var p = Pet{ .id = 1, .number = true, .arr = Array(u8).init(std.heap.page_allocator), .arr2 = [_]u8{ 3, 4, 5 } };
    try p.arr.append(1);
    try p.arr.append(2);

    var json = try toJson(Pet, p, std.heap.page_allocator);
    defer json.object.deinit();
    json.dump();
}

test "to json" {
    const t = std.testing;
    const Pet = struct {
        id: i32,
        number: bool,
    };
    var json = try toJson(Pet, Pet{ .id = 1, .number = true }, std.heap.page_allocator);

    try t.expectEqual(json.object.getEntry("id").?.value_ptr.*, std.json.Value{ .integer = 1 });
    try t.expectEqual(json.object.getEntry("number").?.value_ptr.*, std.json.Value{ .bool = true });
    // try t.expectEqual(json.object.getEntry("number").?.value_ptr.*, true);
}
