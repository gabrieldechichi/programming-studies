const std = @import("std");

pub fn main() !void {
    var v : ?bool = null;
    var p = &v;
    try std.io.getStdOut().writer().print("value: {?}, pointer {?}\n", .{p.*, p});
    p.* = true;
    try std.io.getStdOut().writer().print("value: {?}, pointer {?}\n", .{p.*, p});
    
    // const stdout = std.io.getStdOut().writer();
    // var xoshiro = std.rand.DefaultPrng.init(1234);
    // var rand = xoshiro.random();
    // const random = getRandomNumber(&rand);
    // try stdout.print("Guess the number (1-100): {d}", .{random});
    //
    // while (true){
    //     const guess = try getGuessFromUser();
    //     if (guess < random){
    //         // try stdout.print("Too low! Try again");
    //     } else if (guess > random) {
    //         // try stdout.print("Too high! Try again");
    //     } else {
    //         // try stdout.print("You're right!");
    //         break;
    //     }
    // }
}

fn getRandomNumber(random: *std.rand.Random) u8 {
    return random.uintLessThan(u8, 100) + 1;
}

fn getGuessFromUser() !u8 {
    var buffer: [20]u8 = undefined;
    _ = try std.io.getStdIn().reader().readUntilDelimiterOrEof(&buffer, '\n');
    try std.io.getStdOut().writer().print("{s}", .{buffer});
    return 72;
    // return try std.fmt.parseInt(u8, &buffer, 10);
}
