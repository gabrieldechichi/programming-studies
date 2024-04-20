const std = @import("std");
const rl = @cImport(@cInclude("raylib.h"));

pub fn main() !void {
    rl.InitWindow(960, 540, "My Window Name");
    rl.SetTargetFPS(60);
    defer rl.CloseWindow();

    while (!rl.WindowShouldClose()){
        rl.BeginDrawing();
        defer rl.EndDrawing();
        rl.DrawText("Hello World", 12, 12, 40, rl.RED);
        rl.ClearBackground(rl.BLACK);
    }
}
