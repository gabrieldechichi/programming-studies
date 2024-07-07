package main

import "core:fmt"
import "core:math/linalg"
import "lib"
import wasmjs "vendor:wasm/js"
import "wgpu"

viewProjection: lib.mat4x4
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
	wgpu.debugRendererDrawSquare(linalg.matrix4_scale_f32(linalg.Vector3f32{200, 200, 1}))
	wgpu.render(viewProjection)
	return true
}
