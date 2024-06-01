package main

import "core:fmt"
import "core:math"
import "game"
import rl "vendor:raylib"

Entity :: struct {
	id:       u32,
	position: rl.Vector2,
	variant:  union {
		Player,
	},
	graphics: union {
		GraphicsRect,
	},
}

Player :: struct {}

GraphicsRect :: struct {
	width:  f32,
	height: f32,
	color:  rl.Color,
}

draw_entities :: proc(viewport: game.Viewport2D, entities: #soa[]Entity) {
	for &entity in entities {
		switch graphics in entity.graphics {
		case GraphicsRect:
			{
				s := rl.Vector2{graphics.width, graphics.height}
				p := game.rect_to_viewport(viewport, entity.position, s)
				rl.DrawRectangleV(p, s * viewport.scale, graphics.color)
			}
		}
	}
}

new_entity :: proc(entities: ^#soa[dynamic]Entity, entity: Entity) -> u32 {
	entity := entity
	@(static)
	id: u32 = 0
	id += 1
	entity.id = id
	append_soa(entities, entity)
	return id
}

main :: proc() {
	viewport := game.Viewport2D {
		width  = 1280,
		height = 720,
		scale  = 10,
	}

	entities: #soa[dynamic]Entity
	player_id := new_entity(
		&entities,
		{
			variant = Player{},
			graphics = GraphicsRect{width = 10, height = 10, color = rl.BLUE},
		},
	)

	rl.InitWindow(
		auto_cast viewport.width,
		auto_cast viewport.height,
		"Simple Platformer",
	)

	for !rl.WindowShouldClose() {
		rl.BeginDrawing()
		rl.ClearBackground(rl.BLACK)
		draw_entities(viewport, entities[:])
		rl.EndDrawing()
	}
}
