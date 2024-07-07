package main

import "core:fmt"
import "core:math/linalg"
import wasmjs "vendor:wasm/js"
import "wgpu"

mat4x4 :: linalg.Matrix4x4f32

viewProjection: mat4x4
width: f32 = 600.0
height: f32 = 900.0

main :: proc() {
	viewProjection = linalg.matrix_ortho3d(
		-width / 2,
		width / 2,
		-height / 2,
		height / 2,
		-1,
		1,
	)
}

@(export = true)
step :: proc(dt: f32) -> bool {
	wgpu.render(viewProjection)
	return true
}
