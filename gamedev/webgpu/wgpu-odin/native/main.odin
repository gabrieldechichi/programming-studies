package main

import "core:fmt"
import "core:math/linalg"
import "lib:types"
import "lib:wgpu"
import wasmjs "vendor:wasm/js"

viewProjection: types.mat4x4
width: f32 = 600.0
height: f32 = 900.0
matrices: [256]types.mat4x4

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
	matrices[0] = linalg.matrix4_scale_f32(types.vec3{200, 200, 1})
	matrices[1] = linalg.matrix4_from_trs(
		types.vec3{200, 0, 0},
		linalg.quaternion_from_euler_angle_z(cast(f32)linalg.PI / 4),
		types.vec3{100, 200, 1},
	)
	matrices[2] = linalg.matrix4_from_trs(
		types.vec3{-200, -100, 0},
		linalg.quaternion_from_euler_angle_z(cast(f32)-linalg.PI / 4),
		types.vec3{100, 200, 1},
	)

	wgpu.debugRendererSetUniforms(raw_data(matrices[:]), 3, 16)

	wgpu.render(viewProjection)
	return true
}
