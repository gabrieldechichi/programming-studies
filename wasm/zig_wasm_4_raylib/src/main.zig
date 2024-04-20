const std = @import("std");
const rl = @cImport(@cInclude("raylib.h"));
const rlmath = @cImport(@cInclude("raymath.h"));
const Vector2 = rlmath.Vector2;

const ScreenSize = struct {
    width: f32,
    height: f32,
};

const Ball = struct { pos: Vector2, velocity: Vector2, radius: f32, color: rl.Color };

fn sqred(n: anytype) @TypeOf(n) {
    return n * n;
}

const ScreenPos = struct { x: i32, y: i32 };

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    rl.InitWindow(960, 540, "My Window Name");
    rl.SetTargetFPS(30);
    defer rl.CloseWindow();

    var rnd = std.rand.DefaultPrng.init(0);

    const total_balls = 10000;
    var balls = try std.ArrayList(Ball).initCapacity(allocator, total_balls);
    for (0..total_balls) |_| {
        const color = rl.Color{ .r = rnd.random().uintLessThan(u8, 255), .g = rnd.random().uintLessThan(u8, 255), .b = rnd.random().uintLessThan(u8, 255), .a = 254 };
        const vx: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        const vy: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        var ball = Ball{ .color = color, .radius = 20.0, .pos = Vector2{ .x = 0, .y = 0 }, .velocity = Vector2{ .x = vx, .y = vy } };
        try balls.append(ball);
    }
    var screen_size = ScreenSize{ .width = @floatFromInt(rl.GetScreenWidth()), .height = @floatFromInt(rl.GetScreenHeight()) };

    while (!rl.WindowShouldClose()) {
        //update
        {
            const dt = rl.GetFrameTime();
            for (balls.items) |*ball| {
                if (sqred(ball.pos.x) >= sqred((screen_size.width - ball.radius) / 2)) {
                    ball.velocity.x *= -1;
                }
                if (sqred(ball.pos.y) >= sqred((screen_size.height - ball.radius) / 2)) {
                    ball.velocity.y *= -1;
                }
                ball.pos = rlmath.Vector2Add(ball.pos, rlmath.Vector2Scale(ball.velocity, dt));
            }
        }

        //render
        {
            rl.BeginDrawing();
            defer rl.EndDrawing();
            rl.ClearBackground(rl.BLACK);

            for (balls.items) |ball| {
                var ball_screen_pos = ScreenPos{ .x = @intFromFloat(ball.pos.x + screen_size.width / 2), .y = @intFromFloat(ball.pos.y + screen_size.height / 2) };
                rl.DrawCircle(ball_screen_pos.x, ball_screen_pos.y, ball.radius, ball.color);
            }

            rl.DrawFPS(12, 12);
        }
    }
}
