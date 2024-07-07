package wgpu


@(link_prefix = "wgpu_", default_calling_convention = "c")
foreign _ {
	render :: proc(viewProjection: matrix[4, 4]f32) ---
}
