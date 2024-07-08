package wgpu

import "lib:types"

@(link_prefix = "wgpu_", default_calling_convention = "c")
foreign _ {
	render :: proc(viewProjection: types.mat4x4) ---
	debugRendererAddBatch :: proc(matrices: [^]types.mat4x4, instanceCount: u32, instanceFloatCount: u32) ---
	debugRendererSetBatches :: proc(matrices: [^]types.mat4x4, instanceCount: u32, instanceFloatCount: u32) ---
}
