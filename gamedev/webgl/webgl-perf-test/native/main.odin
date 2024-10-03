package main

import "core:fmt"
import "core:math/linalg"
import gl "vendor:wasm/WebGL"

vec2 :: linalg.Vector2f32
vec3 :: linalg.Vector3f32
vec4 :: linalg.Vector4f32
color :: vec4

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

colors: []color = {
	{1, 0, 0, 1},
	{0, 1, 0, 1},
	{0, 0, 1, 1},
	{1, 1, 0, 1},
	{1, 0, 1, 1},
	{0, 1, 1, 1},
}

vertexShaderSource :: `#version 300 es
void main()
{
    gl_PointSize = 15.0;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}`

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

	program, ok := gl.CreateProgramFromStrings([]string{vertexShaderSource}, []string{fragmentShaderSource})
	if !ok {return}

    gl.UseProgram(program)
    gl.DrawArrays(gl.POINTS, 0, 1);
}
