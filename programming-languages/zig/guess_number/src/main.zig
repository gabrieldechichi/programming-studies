const std = @import("std");

pub fn main() !void {
    const seed = @as(u64, @intCast(std.time.milliTimestamp()));
    var xoshiro = std.rand.DefaultPrng.init(seed);
    var rand = xoshiro.random();
    const random = rand.uintLessThan(u8, 100) + 1;

    try printLine("Guess the number (1-100):", .{});

    while (true) {
        const guess = try getGuessFromUser();
        if (guess < random) {
            try printLine("Too low! Try again", .{});
        } else if (guess > random) {
            try printLine("Too high! Try again", .{});
        } else {
            try printLine("You're right!", .{});
            break;
        }
    }
}

fn printLine(comptime format: []const u8, args: anytype) !void {
    const stdout = std.io.getStdOut().writer();
    try stdout.print(format ++ "\n", args);
}

fn getGuessFromUser() !u8 {
    const in = std.io.getStdIn();
    var br = std.io.bufferedReader(in.reader());
    var reader = br.reader();
    var msg_buffer: [20]u8 = undefined;
    var msg = try reader.readUntilDelimiterOrEof(&msg_buffer, '\n');
    if (msg) |m| {
        return try std.fmt.parseInt(u8, m[0..m.len-1], 10);
    }
    return 0;
}
