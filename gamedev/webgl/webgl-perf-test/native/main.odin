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
QUAD :: []vec2{{-1, -1}, {1, -1}, {1, 1}, {1, 1}, {-1, 1}, {-1, -1}}

@(private = "file")
vertexShaderSource :: `#version 300 es

layout(location=0) in vec2 aVertexPos;

void main()
{
    vec2 pos = vec2(0.5) * aVertexPos;
    gl_Position = vec4(pos, 0,1);
}`

@(private = "file")
fragmentShaderSource :: `#version 300 es
precision mediump float;

out vec4 fragColor;

void main()
{
    fragColor = vec4(1.0, 1.0, 0.0, 1.0);
}`

doInstanceTest :: proc() {
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

	whiteSprite := Sprite {
		texture = whiteTex,
		x       = 0,
		y       = 0,
		w       = 1,
		h       = 1,
	}

	width: f32 = 1080.0
	height: f32 = 1920.0

	viewProjectionMatrix := linalg.matrix_ortho3d(
		-width / 2,
		width / 2,
		-height / 2,
		height / 2,
		-1,
		10,
	)

	ok := spriteRendererDrawSprite(&spriteRenderer, whiteSprite, scale = vec2{50, 50})
	if !ok {return}
	spriteRendererRender(&spriteRenderer, viewProjectionMatrix)
}

doBasicTest :: proc() {
	success := gl.SetCurrentContextById("canvas")
	if !success {return}

	program, _ := glCreateProgramFromStrings(vertexShaderSource, fragmentShaderSource)
	gl.UseProgram(program)

    vao := gl.CreateVertexArray()
    gl.BindVertexArray(vao)

	aVertexPos := gl.GetAttribLocation(program, "aVertexPos")
	vertexBuffer := gl.CreateBuffer()
	gl.BindBuffer(gl.ARRAY_BUFFER, vertexBuffer)
	gl.BufferData(gl.ARRAY_BUFFER, size_of(QUAD[0]) * len(QUAD), raw_data(QUAD), gl.STATIC_DRAW)
	gl.VertexAttribPointer(
		aVertexPos,
		size = 2,
		type = gl.FLOAT,
		normalized = false,
		stride = size_of(vec2),
		ptr = 0,
	)
    gl.EnableVertexAttribArray(aVertexPos)

    gl.BindVertexArray(0)

    gl.BindVertexArray(vao)

	gl.DrawArrays(gl.TRIANGLES, 0, 6)
}

main :: proc() {
	doInstanceTest()
}
