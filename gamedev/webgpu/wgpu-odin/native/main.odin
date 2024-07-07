package main

import "core:fmt"
import "core:math"
import "core:math/linalg"
import "core:math/rand"
import "lib:types"
import "lib:wgpu"
import wasmjs "vendor:wasm/js"

viewProjection: types.mat4x4
width: f32 = 600.0
height: f32 = 900.0
MAX_INSTANCES :: 1024
MAX_SPEED :: 100

balls: #soa[MAX_INSTANCES]Ball

Ball :: struct {
	position:  types.vec2,
	radius:    f32,
	velocity:  types.vec2,
	transform: types.mat4x4,
}

main :: proc() {
	viewProjection = linalg.matrix_ortho3d(
		-width / 2,
		width / 2,
		-height / 2,
		height / 2,
		-1,
		1,
	)

	for i in 0 ..< MAX_INSTANCES {

		x := rand.float32_range(-width / 2, width / 2)
		y := rand.float32_range(-height / 2, height / 2)
		vx := rand.float32_range(-1, 1) * MAX_SPEED
		vy := rand.float32_range(-1, 1) * MAX_SPEED

		balls[i] = {
			position = types.vec2{x, y},
			radius   = 25,
			velocity = types.vec2{vx, vy},
		}
	}
}

@(export)
step :: proc(dt: f32) -> bool {

	for &ball in balls {
		ball.position += ball.velocity * dt
		if (abs(ball.position.x) + ball.radius > width / 2) {
			ball.velocity.x *= -1
		}
		if (abs(ball.position.y) + ball.radius > height / 2) {
			ball.velocity.y *= -1
		}
		ball.transform = linalg.matrix4_from_trs(
			types.vec3{ball.position.x, ball.position.y, 0},
			linalg.QUATERNIONF32_IDENTITY,
			types.vec3{ball.radius * 2, ball.radius * 2, 0},
		)
	}

	wgpu.debugRendererSetUniforms(raw_data(balls.transform[:]), MAX_INSTANCES, 16)

	wgpu.render(viewProjection)
	return true
}
