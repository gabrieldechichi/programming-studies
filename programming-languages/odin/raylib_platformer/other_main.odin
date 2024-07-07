package main

// import "core:fmt"
// import rl "vendor:raylib"
//
// main :: proc() {
// 	rl.InitWindow(1280, 720, "Simple Platformer")
// 	defer rl.CloseWindow()
// 	rl.SetTargetFPS(60)
//
//
// 	// camera := rl.Camera3D {
// 	// 	position   = {10, 0, 0},
// 	// 	target     = {0, 0, 0},
// 	// 	up         = {0, 1, 0},
// 	// 	fovy       = 45,
// 	// 	projection = rl.CameraProjection.PERSPECTIVE,
// 	// }
//
// 	camera := rl.Camera2D {
// 		offset = {
// 			auto_cast rl.GetScreenWidth() / 2,
// 			auto_cast rl.GetScreenHeight() / 2,
// 		},
// 		target = {0, -1},
// 		zoom   = 1,
// 	}
// 	p := rl.Vector2{}
//
//
// 	for !rl.WindowShouldClose() {
// 		defer free_all(context.temp_allocator)
//
// 		if rl.IsKeyDown(rl.KeyboardKey.A) {
// 			p.y -= 10
// 		}
// 		if rl.IsKeyDown(rl.KeyboardKey.D) {
// 			p.y += 10
// 		}
// 		rl.BeginDrawing()
// 		rl.ClearBackground(rl.WHITE)
//
// 		rl.BeginMode2D(camera)
// 		// rl.DrawCube({0, 0, 0}, 2, 2, 2, rl.RED)
// 		rl.DrawRectangleV(p, {100, 100}, rl.RED)
// 		rl.EndMode2D()
// 		rl.DrawFPS(10, 10)
// 		rl.EndDrawing()
// 	}
// }
