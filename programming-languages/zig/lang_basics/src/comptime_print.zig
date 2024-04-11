const std = @import("std");

const debug_print = std.debug.print;

fn fibbonaci(index: u64) u64 {
    if (index < 2) {
        return index;
    }
    return fibbonaci(index - 1) + fibbonaci(index - 2);
}

pub fn comptime_fibbonnaci() void {
    const r: u64 = comptime fibbonaci(51);
    debug_print("{}", .{r});
}

const Node = struct {
    next: ?*Node,
    name: []const u8,
};

pub fn generic_nodes_test() void {
    var root = Node{ .next = null, .name = "root" };
    var a = Node{
        .next = null,
        .name = "a",
    };
    var b = Node{
        .next = null,
        .name = "b",
    };

    root.next = &a;
    a.next = &b;

    var current: ?Node = root;
    debug_print("Node {s}\n", .{current.?.name});
    while (current.?.next != null) {
        debug_print("Node {s}\n", .{current.?.next.?.name});
        current = current.?.next.?.*;
    }
}

pub fn simple_print() !void {
    const x = 8;
    var w = Writer{};
    try w.print("I wrote a print function in zig. {}. And it works!", .{x});
}

const Writer = struct {
    pub fn print(self: *Writer, comptime format: []const u8, args: anytype) anyerror!void {
        const State = enum { start, open_brace };

        comptime var start_index: usize = 0;
        comptime var state = State.start;
        comptime var arg_index: usize = 0;

        inline for (format, 0..) |c, i| {
            switch (state) {
                State.start => switch (c) {
                    '{' => {
                        try self.write(format[start_index..i]);
                        state = State.open_brace;
                    },
                    else => {},
                },
                State.open_brace => switch (c) {
                    '}' => {
                        if (arg_index >= args.len){
                            @compileError("Missing arguments");
                        }
                        try self.printValue(args[arg_index]);
                        arg_index += 1;
                        state = State.start;
                        start_index = i + 1;
                    },
                    else => @compileError("Unknown format character: " ++ [_]u8{c}),
                },
            }
        }

        comptime {
            if (args.len != arg_index) {
                @compileError("Unused arguments");
            }
            if (state != State.start) {
                @compileError("Incomplete format string: " ++ format);
            }
        }

        if (start_index < format.len) {
            try self.write(format[start_index..format.len]);
        }
    }

    fn write(self: *Writer, comptime format: []const u8) !void {
        _ = self;
        debug_print(format, .{});
    }

    fn printValue(self: *Writer, value: anytype) !void {
        _ = self;
        debug_print("{}", .{value});
    }
};
