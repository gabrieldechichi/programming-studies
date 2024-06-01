package game

import rl "vendor:raylib"

Viewport2D :: struct {
	width:  f32,
	height: f32,
	scale:  f32,
}

world_to_viewport :: proc(viewport: Viewport2D, p: rl.Vector2) -> rl.Vector2 {
	return rl.Vector2{p.x + viewport.width / 2, -p.y + viewport.height / 2}
}

rect_to_viewport :: proc(
	viewport: Viewport2D,
	p: rl.Vector2,
	size: rl.Vector2,
) -> rl.Vector2 {
	return rl.Vector2 {
		p.x - viewport.scale * size.x / 2 + viewport.width / 2,
		-p.y - viewport.scale * size.y / 2 + viewport.height / 2,
	}
}
