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
MAX_INSTANCES :: BATCH_SIZE * 500
BATCH_SIZE :: 1024
MAX_SPEED :: 200

balls: #soa[]Ball

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

	balls = make_soa(#soa[]Ball, MAX_INSTANCES)

	rand.reset(1234)
	for i in 0 ..< len(balls) {

		x := rand.float32_range(-width / 2, width / 2)
		y := rand.float32_range(-height / 2, height / 2)
		vx := rand.float32_range(-1, 1) * MAX_SPEED
		vy := rand.float32_range(-1, 1) * MAX_SPEED

		balls[i] = {
			position = types.vec2{x, y},
			radius   = 5,
			velocity = types.vec2{vx, vy},
		}
	}
}

@(export)
step :: proc(dt: f32) -> bool {

	for &ball in balls {
		ball.position += ball.velocity * dt
		if (ball.position.x + ball.radius > width / 2 && ball.velocity.x > 0) {
			ball.velocity.x *= -1
		} else if (ball.position.x - ball.radius < -width / 2 &&
			   ball.velocity.x < 0) {
			ball.velocity.x *= -1
		}
		if (ball.position.y + ball.radius > height / 2 &&
			   ball.velocity.y > 0) {
			ball.velocity.y *= -1
		} else if (ball.position.y - ball.radius < -height / 2 &&
			   ball.velocity.y < 0) {
			ball.velocity.y *= -1
		}

		ball.transform = linalg.matrix4_from_trs(
			types.vec3{ball.position.x, ball.position.y, 0},
			linalg.QUATERNIONF32_IDENTITY,
			types.vec3{ball.radius * 2, ball.radius * 2, 0},
		)
	}

	wgpu.debugRendererSetBatches(
		raw_data(balls.transform[0:len(balls)]),
		cast(u32)len(balls),
		16,
	)

	wgpu.render(viewProjection)
	return true
}
