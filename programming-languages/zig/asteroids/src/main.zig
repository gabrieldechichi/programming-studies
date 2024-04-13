const std = @import("std");
const rl = @import("raylib");
const rlm = @import("raylib-math");

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
const SHIP_SCALE = 15;

const SHIP_POINTS = [_]rl.Vector2{
    rl.Vector2.init(-SHIP_WIDTH, -SHIP_HEIGHT),
    rl.Vector2.init(0, SHIP_HEIGHT),
    rl.Vector2.init(SHIP_WIDTH, -SHIP_HEIGHT),
    rl.Vector2.init(-SHIP_WIDTH, -SHIP_HEIGHT),
};

const Ship = struct {
    transform: Transform2D,

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
    viewport: Viewport,
    ship: Ship,
};

var state: State = undefined;

pub fn main() !void {
    std.debug.print("Hello world", .{});
    rl.initWindow(1280, 720, "ZIGSTEROIDS");
    rl.setTargetFPS(60);

    state = State{
        .viewport = Viewport{ .screen_size = rl.Vector2.init(@floatFromInt(rl.getScreenWidth()), @floatFromInt(rl.getScreenHeight())) },
        .ship = Ship{ .transform = Transform2D{
            .pos = rlm.vector2Zero(),
            .rot = 0.0,
            .uniform_scale = SHIP_SCALE,
        } },
    };

    while (!rl.windowShouldClose()) {
        rl.beginDrawing();
        defer rl.endDrawing();

        rl.clearBackground(rl.Color.black);

        state.ship.draw();
    }
}
