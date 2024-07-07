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

@(export)
step :: proc(dt: f32) -> bool {
	wgpu.debugRendererDrawSquare(
		linalg.matrix4_scale_f32(lib.vec3{200, 200, 1}),
	)
	wgpu.debugRendererDrawSquare(
		linalg.matrix4_from_trs(
			lib.vec3{200, 0, 0},
			linalg.quaternion_from_euler_angle_z(cast(f32)linalg.PI / 4),
			lib.vec3{100, 200, 1},
		),
	)

	wgpu.debugRendererDrawSquare(
		linalg.matrix4_from_trs(
			lib.vec3{-200, -100, 0},
			linalg.quaternion_from_euler_angle_z(cast(f32)-linalg.PI / 4),
			lib.vec3{100, 200, 1},
		),
	)
	wgpu.render(viewProjection)
	return true
}
