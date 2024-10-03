package main

import "core:fmt"
import "core:math/linalg"
import "core:math/rand"
import gl "vendor:wasm/WebGL"


Ball :: struct {
	position:   vec2,
	radius:     f32,
	velocity:   vec2,
	colorIndex: i32,
}

BATCH_SIZE :: 1024 * 1
MAX_INSTANCES :: BATCH_SIZE * 1 - 10
MAX_SPEED :: 200

balls: #soa[]Ball

colors: []vec4 = {
	{1, 0, 0, 1},
	{0, 1, 0, 1},
	{0, 0, 1, 1},
	{1, 1, 0, 1},
	{1, 0, 1, 1},
	{0, 1, 1, 1},
}
whiteSprite: Sprite
width: f32 = 500
height: f32 = 500
viewProjectionMatrix := linalg.matrix_ortho3d(
	-width / 2.,
	width / 2.,
	-height / 2.,
	height / 2.,
	-1,
	10,
)
spriteRenderer: SpriteRenderer

main :: proc() {
	//setup graphics
	{
		success := gl.CreateCurrentContextById("canvas", {.disableAntialias})
		if !success {return}

		err: WebGLError
		spriteRenderer, err = spriteRendererNew(BATCH_SIZE)
		if err != .None {
			return
		}

		pixels := [4]byte{255, 255, 255, 255}
		whiteTex := spriteRendererAddTexturePixels(
			spriteRenderer = &spriteRenderer,
			pixels = pixels[:],
			width = 1,
			height = 1,
		)

		whiteSprite = Sprite {
			texture = whiteTex,
			x       = 0,
			y       = 0,
			w       = 1,
			h       = 1,
		}

		viewProjectionMatrix = linalg.matrix_ortho3d(
			-width / 2,
			width / 2,
			-height / 2,
			height / 2,
			-1,
			10,
		)
		// gl.Viewport(0, 0, auto_cast width, auto_cast height)
	}

	//setup balls
	{
		balls = make_soa(#soa[]Ball, MAX_INSTANCES)

		rand.reset(1234)
		for i in 0 ..< len(balls) {

			x := rand.float32_range(-width / 2, width / 2)
			y := rand.float32_range(-height / 2, height / 2)
			vx := rand.float32_range(-1, 1) * MAX_SPEED
			vy := rand.float32_range(-1, 1) * MAX_SPEED

			balls[i] = {
				position   = vec2{x, y},
				radius     = 5,
				velocity   = vec2{vx, vy},
				colorIndex = auto_cast rand.int_max(len(colors)),
			}
		}
	}
}

lastTime: f64
@(export)
step :: proc(currentTime: f64) -> (keep_going: bool) {
	dt := f32(currentTime - lastTime)
	lastTime = currentTime
	//update
	{
		for &ball in balls {
			ball.position += ball.velocity * dt
			if (ball.position.x + ball.radius > width / 2 && ball.velocity.x > 0) {
				ball.velocity.x *= -1
			} else if (ball.position.x - ball.radius < -width / 2 && ball.velocity.x < 0) {
				ball.velocity.x *= -1
			}
			if (ball.position.y + ball.radius > height / 2 && ball.velocity.y > 0) {
				ball.velocity.y *= -1
			} else if (ball.position.y - ball.radius < -height / 2 && ball.velocity.y < 0) {
				ball.velocity.y *= -1
			}
		}
	}

	//render
	{

		for ball in balls {
			using ball
			spriteRendererDrawSprite(
				&spriteRenderer,
				whiteSprite,
				pos = vec3{position.x, position.y, 0},
				scale = vec2{radius, radius},
				color = colors[colorIndex],
			)
		}
		spriteRendererRender(&spriteRenderer, viewProjectionMatrix)
	}
	return true
}
