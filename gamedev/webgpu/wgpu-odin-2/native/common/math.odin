package common

import "core:math/linalg"

mat4x4 :: linalg.Matrix4x4f32
vec3 :: linalg.Vector3f32
vec2 :: linalg.Vector2f32

Color :: struct {
	r: f32,
	g: f32,
	b: f32,
	a: f32,
}

color :: proc(r: f32 = 0, g: f32 = 0, b: f32 = 0, a: f32 = 1) -> Color {
	return Color{r = r, g = g, b = b, a = a}
}
