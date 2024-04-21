const std = @import("std");
const rl = @cImport(@cInclude("raylib.h"));
const rlmath = @cImport(@cInclude("raymath.h"));
const Vector2 = rlmath.Vector2;

const ScreenSize = struct {
    width: u16,
    height: u16,

    pub fn width_f(self: *ScreenSize) f32 {
        return @floatFromInt(self.width);
    }
    pub fn height_f(self: *ScreenSize) f32 {
        return @floatFromInt(self.height);
    }
};

// const Ball = struct { pos: Vector2, velocity: Vector2, radius: f32, color: rl.Color };
const Ball = struct { x: f32, y: f32, vx: f32, vy: f32, radius: f32, color: rl.Color };

const ScreenPos = struct { x: i32, y: i32 };

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    rl.InitWindow(960, 540, "My Window Name");
    rl.SetTargetFPS(30);
    defer rl.CloseWindow();

    var perf_stats_buffer: [11]u8 = undefined;

    var screen_size = ScreenSize{ .width = @intCast(rl.GetScreenWidth()), .height = @intCast(rl.GetScreenHeight()) };

    var rnd = std.rand.DefaultPrng.init(0);

    const batch_size = 8;
    const total_balls = batch_size * 1024;
    var balls = try std.ArrayList(Ball).initCapacity(allocator, total_balls);
    for (0..total_balls) |_| {
        const color = rl.Color{ .r = rnd.random().uintLessThan(u8, 255), .g = rnd.random().uintLessThan(u8, 255), .b = rnd.random().uintLessThan(u8, 255), .a = 254 };
        const vx: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        const vy: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        var ball = Ball{ .color = color, .radius = 20.0, .x = 0, .y = 0, .vx = vx, .vy = vy };
        try balls.append(ball);
    }

    while (!rl.WindowShouldClose()) {
        //update
        {
            const dt = rl.GetFrameTime();
            for (balls.items) |*ball| {
                if (ball.x - ball.radius < -screen_size.width_f() / 2 or ball.x + ball.radius > screen_size.width_f() / 2) {
                    ball.vx *= -1;
                }
                if (ball.y - ball.radius < -screen_size.height_f() / 2 or ball.y + ball.radius > screen_size.height_f() / 2) {
                    ball.vy *= -1;
                }
                ball.x += ball.vx * dt;
                ball.y += ball.vy * dt;
            }
        }

        //render
        {
            rl.BeginDrawing();

            {
                rl.ClearBackground(rl.BLACK);
                for (balls.items) |ball| {
                    rl.DrawCircle(@intFromFloat(ball.x + screen_size.width_f()/2), @intFromFloat(ball.y + screen_size.height_f()/2), ball.radius, ball.color);
                }
                const stats = try std.fmt.bufPrint(&perf_stats_buffer, "Dt: {d:.2}ms", .{rl.GetFrameTime() * 1000});
                rl.DrawText(stats.ptr, 12, 12, 20, rl.ORANGE);

                rl.EndTextureMode();
            }

            rl.EndDrawing();
        }
    }
}
