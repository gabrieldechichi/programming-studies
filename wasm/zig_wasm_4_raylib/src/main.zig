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

const BallColor = enum { one, two, three };
// const Ball = struct { pos: Vector2, velocity: Vector2, radius: f32, color: rl.Color };
const Ball = struct { x: f32, y: f32, vx: f32, vy: f32, color: BallColor };

const ScreenPos = struct { x: i32, y: i32 };

const FrameStats = struct {
    const entry_count = 100;
    dts: [entry_count]f32,
    avg_dt: f32,
    last_entry: usize,

    pub fn record(self: *FrameStats) void {
        self.dts[self.last_entry] = rl.GetFrameTime();
        self.last_entry = (self.last_entry + 1) % entry_count;
        var s: f32 = 0.0;
        for (self.dts) |dt| {
            s += dt;
        }
        self.avg_dt = s / entry_count;
    }

    pub fn frameTimeMs(self: *const FrameStats) f32 {
        return self.avg_dt * 1000;
    }
};

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    rl.InitWindow(960, 540, "My Window Name");
    rl.SetTargetFPS(10000);
    defer rl.CloseWindow();

    var frame_stats = FrameStats{
        .dts = undefined,
        .avg_dt = 0,
        .last_entry = 0,
    };
    var perf_stats_buffer: [25]u8 = undefined;

    var screen_size = ScreenSize{ .width = @intCast(rl.GetScreenWidth()), .height = @intCast(rl.GetScreenHeight()) };

    var rnd = std.rand.DefaultPrng.init(0);

    const ball_radius = 20.0;
    const batch_size = 64  + 32;
    const total_balls = batch_size * 1024;
    var balls = try std.ArrayList(Ball).initCapacity(allocator, total_balls);
    for (0..total_balls) |_| {
        const color = switch (rnd.random().uintLessThan(u8, 3)) {
            0 => BallColor.one,
            1 => BallColor.two,
            else => BallColor.three,
        };
        const vx: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        const vy: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        var ball = Ball{ .color = color, .x = 0, .y = 0, .vx = vx, .vy = vy };
        try balls.append(ball);
    }

    while (!rl.WindowShouldClose()) {
        //update
        {
            const dt = rl.GetFrameTime();
            for (balls.items) |*ball| {
                if (ball.x - ball_radius < -screen_size.width_f() / 2 or ball.x + ball_radius > screen_size.width_f() / 2) {
                    ball.vx *= -1;
                }
                if (ball.y - ball_radius < -screen_size.height_f() / 2 or ball.y + ball_radius > screen_size.height_f() / 2) {
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
                    const c = switch (ball.color) {
                        .one => rl.RED,
                        .two => rl.BLUE,
                        .three => rl.GREEN,
                    };
                    rl.DrawRectangle(
                        @intFromFloat(ball.x + screen_size.width_f() / 2),
                        @intFromFloat(ball.y + screen_size.height_f() / 2),
                        @intFromFloat(ball_radius * 2.0),
                        @intFromFloat(ball_radius * 2.0),
                        c,
                    );
                }
                frame_stats.record();
                const stats = try std.fmt.bufPrint(
                    &perf_stats_buffer,
                    "Dt: {d:.2}ms",
                    .{ frame_stats.frameTimeMs()},
                );
                rl.DrawText(stats.ptr, 12, 12, 20, rl.ORANGE);

                rl.EndTextureMode();
            }

            rl.EndDrawing();
        }
    }
}
