const std = @import("std");
const rl = @import("raylib");
const rlm = @import("raylib-math");
const Allocator = std.mem.Allocator;

//TODO:
// Ship + movement
// Asteroids + movement
// Ship hit
// Sounds
// Game state (lives + score)
// UI (lives + score)

const Transform2D = struct {
    pos: rl.Vector2,
    rot: f32,
    uniform_scale: f32,

    fn transformPos(self: Transform2D, pos: rl.Vector2) rl.Vector2 {
        return rlm.vector2Add(rlm.vector2Scale(rlm.vector2Rotate(pos, self.rot), self.uniform_scale), self.pos);
    }
};

const SHIP_WIDTH = 1.0;
const SHIP_HEIGHT = 1.3;
const SHIP_TICKNESS = 2.5;

const SHIP_POINTS = [_]rl.Vector2{
    rl.Vector2.init(-SHIP_WIDTH, -SHIP_HEIGHT),
    rl.Vector2.init(0, SHIP_HEIGHT),
    rl.Vector2.init(SHIP_WIDTH, -SHIP_HEIGHT),
    rl.Vector2.init(-SHIP_WIDTH, -SHIP_HEIGHT),
};

const ShipData = struct { scale: f32 };

const Ship = struct {
    transform: Transform2D,
    data: ShipData,

    fn draw(self: Ship) void {
        const viewport = state.viewport;
        for (0..SHIP_POINTS.len - 1) |i| {
            const from = viewport.to_screen(self.transform.transformPos(SHIP_POINTS[i]));
            const to = viewport.to_screen(self.transform.transformPos(SHIP_POINTS[i + 1]));
            rl.drawLineEx(from, to, SHIP_TICKNESS, rl.Color.white);
        }
    }
};

const Viewport = struct {
    screen_size: rl.Vector2,

    fn to_screen(self: Viewport, pos: rl.Vector2) rl.Vector2 {
        return rl.Vector2.init(pos.x + self.screen_size.x / 2, -pos.y + self.screen_size.y / 2);
    }
};

const State = struct {
    allocator: std.mem.Allocator,
    viewport: Viewport,
    ship: Ship,
};

var state: State = undefined;

pub fn main() !void {
    rl.initWindow(1280, 720, "ZIGSTEROIDS");
    rl.setTargetFPS(60);

    state = State{
        .allocator = std.heap.page_allocator,
        .viewport = Viewport{ .screen_size = rl.Vector2.init(@floatFromInt(rl.getScreenWidth()), @floatFromInt(rl.getScreenHeight())) },
        .ship = Ship{
            .transform = Transform2D{
                .pos = rlm.vector2Zero(),
                .rot = 0.0,
                .uniform_scale = 0.0,
            },
            .data = ShipData{ .scale = 0.0 },
        },
    };

    try reload_data();

    while (!rl.windowShouldClose()) {
        rl.beginDrawing();
        defer rl.endDrawing();

        try update();

        rl.clearBackground(rl.Color.black);

        state.ship.draw();
    }
}

fn update() !void {
    if (rl.isKeyPressed(rl.KeyboardKey.key_f1)) {
        try reload_data();
    }
}

fn dataPath(comptime sub_path: []const u8) []const u8 {
    const data_path = "./assets/data/";
    return comptime data_path ++ sub_path;
}

fn reload_data() !void {
    const ship_data = try loadJson(ShipData, state.allocator, dataPath("ship.json"));
    defer ship_data.deinit();
    state.ship.data = ship_data.value;
    state.ship.transform.uniform_scale = ship_data.value.scale;
}

fn loadJson(comptime T: type, allocator: std.mem.Allocator, path: []const u8) !std.json.Parsed(T) {
    var file = try std.fs.cwd().openFile(path, .{});
    defer file.close();
    var buffered = std.io.bufferedReader(file.reader());
    var reader = std.json.reader(allocator, buffered.reader());
    defer reader.deinit();
    const parsed = try std.json.parseFromTokenSource(T, allocator, &reader, std.json.ParseOptions{ .allocate = std.json.AllocWhen.alloc_always });
    return parsed;
}

fn dumpFile(path: []const u8) !void {
    var file = try std.fs.cwd().openFile(path, .{});
    defer file.close();

    var buffered = std.io.bufferedReader(file.reader());
    var stream = buffered.reader();
    var buffer: [4096]u8 = undefined;
    std.debug.print("\n{s}: \n", .{path});
    while (try stream.readUntilDelimiterOrEof(&buffer, '\n')) |line| {
        std.debug.print("{s}\n", .{line});
    }
}

test "load json" {
    const allocator = std.testing.allocator;
    const data = try loadJson(ShipData, allocator, "./assets/data/ship.json");
    defer data.deinit();
    //no expect needed, just making sure function succeeds
}
