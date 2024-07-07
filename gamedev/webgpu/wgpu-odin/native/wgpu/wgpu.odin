package wgpu

import "../lib"

@(link_prefix = "wgpu_", default_calling_convention = "c")
foreign _ {
	render :: proc(viewProjection: lib.mat4x4) ---
	debugRendererDrawSquare :: proc(modelMatrix: lib.mat4x4) ---
}
