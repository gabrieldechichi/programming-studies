const std = @import("std");
const rl = @cImport(@cInclude("raylib.h"));
const Vector2 = rl.Vector2;

const ScreenSize = struct {
    width: f32,
    height: f32,
};

const Ball = struct { pos: Vector2, velocity: Vector2, radius: f32 };

fn sqred(n: anytype) @TypeOf(n) {
    return n * n;
}

const ScreenPos = struct { x: c_int, y: c_int };

pub fn main() !void {
    rl.InitWindow(960, 540, "My Window Name");
    rl.SetTargetFPS(60);
    defer rl.CloseWindow();

    var ball = Ball{ .radius = 20.0, .pos = Vector2{ .x = 0, .y = 0 }, .velocity = Vector2{ .x = 150, .y = -200 } };
    var screen_size = ScreenSize{ .width = @floatFromInt(rl.GetScreenWidth()), .height = @floatFromInt(rl.GetScreenHeight()) };

    while (!rl.WindowShouldClose()) {
        //update
        {
            const dt = rl.GetFrameTime();
            if (sqred(ball.pos.x) >= sqred((screen_size.width - ball.radius) / 2)) {
                ball.velocity.x *= -1;
            }
            if (sqred(ball.pos.y) >= sqred((screen_size.height - ball.radius) / 2)) {
                ball.velocity.y *= -1;
            }
            ball.pos.x += ball.velocity.x * dt;
            ball.pos.y += ball.velocity.y * dt;
        }

        //render
        {
            rl.BeginDrawing();
            defer rl.EndDrawing();
            rl.ClearBackground(rl.BLACK);
            var ball_screen_pos = ScreenPos{ .x = @intFromFloat(ball.pos.x + screen_size.width / 2), .y = @intFromFloat(ball.pos.y + screen_size.height / 2) };
            rl.DrawCircle(ball_screen_pos.x, ball_screen_pos.y, ball.radius, rl.RED);
        }
    }
}
