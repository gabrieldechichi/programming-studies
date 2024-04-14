const std = @import("std");
const rl = @import("raylib");
const rlm = @import("raylib-math");
const Allocator = std.mem.Allocator;

//TODO: use leaky version and own allocator
const Managed = std.json.Parsed;

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

const ShipData = struct { scale: f32, thickness: f32, points: []rl.Vector2 };

const Ship = struct {
    transform: Transform2D,
    data: ?Managed(ShipData),

    fn draw(self: Ship) void {
        if (self.data) |ship_data| {
            const viewport = state.viewport;
            for (0..ship_data.value.points.len - 1) |i| {
                const fromLocal = rl.Vector2.init(ship_data.value.points[i].x, ship_data.value.points[i].y);
                const toLocal = rl.Vector2.init(ship_data.value.points[i + 1].x, ship_data.value.points[i + 1].y);
                const from = viewport.to_screen(self.transform.transformPos(fromLocal));
                const to = viewport.to_screen(self.transform.transformPos(toLocal));
                rl.drawLineEx(from, to, ship_data.value.thickness, rl.Color.white);
            }
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
var assets: AssetDatabase = undefined;

const AssetDatabase = struct {
    dir: std.fs.IterableDir,
    allocator: Allocator,
    last_update: i128,
    update_delay_ns: i128,

    fn init(allocator: Allocator) !AssetDatabase {
        const update_rate_s = 10;
        const update_delay_ns = 1_000_000_000 / update_rate_s;
        return AssetDatabase{ .dir = try std.fs.cwd().openIterableDir("./assets/data", .{}), .allocator = allocator, .last_update = std.time.nanoTimestamp(), .update_delay_ns = update_delay_ns };
    }

    fn deinit(self: AssetDatabase) void {
        self.dir.close();
    }

    fn shouldRefresh(self: *AssetDatabase) !bool {
        var dirWalker = try self.dir.walk(state.allocator);
        defer dirWalker.deinit();

        const now = std.time.nanoTimestamp();

        if (now - self.last_update < self.update_delay_ns) {
            return false;
        }

        defer self.last_update = now;

        while (try dirWalker.next()) |entry| {
            switch (entry.kind) {
                .file => {
                    const stat = try entry.dir.statFile(entry.path);
                    if (stat.mtime > self.last_update) {
                        return true;
                    }
                },
                else => {},
            }
        }
        return false;
    }
};

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
                .uniform_scale = undefined,
            },
            .data = null,
        },
    };

    assets = try AssetDatabase.init(std.heap.page_allocator);

    try reload_data();

    while (!rl.windowShouldClose()) {
        rl.beginDrawing();
        defer rl.endDrawing();

        //update
        {
            try update();
        }

        //render
        {
            rl.clearBackground(rl.Color.black);
            state.ship.draw();
        }

        //asset update
        {
            if (try assets.shouldRefresh()) {
                reload_data() catch |err| {
                    std.debug.print("Failed to reload data: {}", .{err});
                };
            }
        }
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
    const new_ship_data = try loadJson(ShipData, state.allocator, dataPath("ship.json"));
    if (state.ship.data) |ship_data| {
        ship_data.deinit();
    }
    state.ship.data = new_ship_data;
    state.ship.transform.uniform_scale = state.ship.data.?.value.scale;
}

fn loadJson(comptime T: type, allocator: std.mem.Allocator, path: []const u8) !Managed(T) {
    var file = try std.fs.cwd().openFile(path, .{});
    defer file.close();
    var buffered = std.io.bufferedReader(file.reader());
    var reader = std.json.reader(allocator, buffered.reader());
    defer reader.deinit();
    return try std.json.parseFromTokenSource(T, allocator, &reader, std.json.ParseOptions{ .allocate = std.json.AllocWhen.alloc_always });
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
