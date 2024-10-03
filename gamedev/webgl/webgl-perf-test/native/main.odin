package main

import "core:fmt"
import "core:math/linalg"
import gl "vendor:wasm/WebGL"


Ball :: struct {
	position: vec2,
	radius:   f32,
	velocity: vec2,
}

Viewport :: struct {
	width:  f32,
	height: f32,
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

@(private = "file")
vertexShaderSource :: `#version 300 es
void main()
{
    gl_PointSize = 15.0;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}`

@(private = "file")
fragmentShaderSource :: `#version 300 es
precision mediump float;

out vec4 fragColor;

void main()
{
    fragColor = vec4(1.0, 1.0, 0.0, 1.0);
}`

main :: proc() {
	success := gl.SetCurrentContextById("canvas")
	if !success {return}

	spriteRenderer, err := spriteRendererNew(BATCH_SIZE)
	if err != .None {
		return
	}

	pixels := [4]byte{1, 1, 1, 1}
	whiteTex := spriteRendererAddTexturePixels(
		spriteRenderer = &spriteRenderer,
		pixels = pixels[:],
		width = 1,
		height = 1,
	)
}
