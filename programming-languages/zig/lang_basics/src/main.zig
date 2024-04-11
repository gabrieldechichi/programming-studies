const std = @import("std");
const expect = std.testing.expect;
const print = std.debug.print;

pub fn main() !void {
    // try writeToStdout();
    // try readInput();
    // comptimeStuff();
    // arrays();
    // pointers();
    slices();
    // forLoops();
    // try arrayLists();
}

pub fn writeToStdout() !void {
    const out = std.io.getStdOut();
    var bw = std.io.bufferedWriter(out.writer());
    const stdout = bw.writer();
    try stdout.print("Output to stdout", .{});
    try bw.flush();
}

pub fn readInput() !void {
    const in = std.io.getStdIn();
    var buf = std.io.bufferedReader(in.reader());
    var r = buf.reader();
    var msg_buf: [20]u8 = undefined;
    const msg = try r.readUntilDelimiterOrEof(&msg_buf, '\n');
    if (msg) |m| {
        std.debug.print("msg: {s}\n", .{m});
    }
}

pub fn comptimeStuff() void {
    const f: comptime_float = 15.0;
    print("{}", .{f});
}

pub fn arrays() void {
    const a = [_]u8{ 1, 2, 3 }; //infered size
    const b = [_]u8{ 4, 5, 6 };
    print("len: {}\n", .{a.len});
    print("arr: {any}\n", .{a});
    print("arr: {any}\n", .{a ** 2}); //repeats array
    print("arr: {any}\n", .{a ++ b}); //appends arrays
}

pub fn pointers() void {
    //pointer to non null
    var a = false;
    var pa = &a;
    print("value: {}, pointer {}\n", .{ pa.*, pa });
    pa.* = true;
    print("value: {}, pointer {}\n", .{ pa.*, pa });

    //pointer to null
    var b: ?bool = null;
    var pb = &b;
    print("value: {?}, pointer {?}\n", .{ pb.*, pb });
    pb.* = true;
    print("value: {?}, pointer {?}\n", .{ pb.*, pb });

    //pointer to conts
    const c = true;
    var pc = &c;
    print("value: {?}, pointer {?}\n", .{ pc.*, pc });
    //errror: cannot assign to constant
    // pc.* = false;

    //can make pc point to something else though
    pc = &a;
    print("value: {?}, pointer {?}\n", .{ pc.*, pc });
}

fn slices() void {
    //subset of array. Implementation is pointer + size
    const array = [_]u8{ 1, 2, 3, 4, 5 };

    const slice = array[1..3];
    //this type is actually *const [2]u8, since the size is known at compile time
    print("{any}\n", .{@TypeOf(slice)});
    print("{any}\n", .{slice}); //prints pointer address
    print("{}\n", .{slice.len}); //prints len
    for (slice) |elem| {
        print("element {}, ", .{elem});
    }


    //when size can't be known at compile time
    var xoshiro = std.rand.DefaultPrng.init(1234);
    var rand = xoshiro.random();
    const random_end = rand.uintLessThan(u8, array.len);
    const slice_runtime = array[0..random_end];
    print("{any}\n", .{@TypeOf(slice_runtime)});
    print("{any}\n", .{slice_runtime}); //prints pointer address
    print("{}\n", .{slice_runtime.len}); //prints len
    for (slice_runtime) |elem| {
        print("element {}, ", .{elem});
    }
}

fn forLoops() void {
    const array = [_]u8{ 1, 2, 3, 4, 5 };

    //iterate over elements values
    for (array) |elem| {
        print("{}\n", .{elem});
    }

    print("\n\n", .{});
    //iterate by reference
    for (&array) |*elem| {
        print("{}\n", .{elem});
    }

    print("\n\n", .{});

    //iterate with index
    for (0.., array) |i, elem| {
        print("{}, {}\n", .{i, elem});
    }
}

fn arrayLists() !void {
    var list = std.ArrayList(u8).init(std.heap.page_allocator);
    defer list.deinit();

    print("{}\n", .{@TypeOf(list)});
    try list.append(0);
    try list.append(5);
    try list.append(5);
    try list.append(5);
    try list.append(5);
    print("{}\n", .{list});//prints the pointer
    for (list.items) |e| {
        print("{}\n", .{e});
    }
}

test "always succeed" {
    try expect(true);
}
