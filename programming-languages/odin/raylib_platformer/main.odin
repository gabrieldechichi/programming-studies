package main

import "core:fmt"
import "core:math"
import "game"
import rl "vendor:raylib"

GROUND_LEVEL :: -20
GRAVITY :: 2000

World :: struct {
	players: #soa[dynamic]Player,
}

Entity :: struct {
	id:       u32,
	position: rl.Vector2,
}

Graphics :: struct {
	variant: union {
		GraphicsNone,
		GraphicsRect,
	},
}

Player :: struct {
	using graphics: Graphics,
	using entity:   Entity,
	velocity:       rl.Vector2,
	movement_speed: f32,
	jump_speed:     f32,
	grounded:       bool,
}

GraphicsNone :: struct {}

GraphicsRect :: struct {
	width:  f32,
	height: f32,
	color:  rl.Color,
}

player_update :: proc(players: #soa[]Player) {
	dt := rl.GetFrameTime()
	for &player in players {
		//input
		{
			player.velocity.x = 0
			if rl.IsKeyDown(rl.KeyboardKey.A) {
				player.velocity.x = -player.movement_speed
			}
			if rl.IsKeyDown(rl.KeyboardKey.D) {
				player.velocity.x = player.movement_speed
			}

			if player.grounded && rl.IsKeyPressed(rl.KeyboardKey.SPACE) {
				player.velocity.y = player.jump_speed
				player.grounded = false
			}
		}

		//"physics"
		{
			using player.entity
			player.velocity.y -= GRAVITY * dt
			position += player.velocity * dt
			position.y = math.max(position.y, GROUND_LEVEL)
			player.grounded = position.y <= GROUND_LEVEL
		}
	}
}

world_update :: proc(world: ^World) {
	player_update(world.players[:])
}

world_draw :: proc(viewport: game.Viewport2D, world: ^World) {
	draw_entities_internal :: proc(
		length: int,
		graphics_ptr: [^]Graphics,
		entity_ptr: [^]Entity,
		viewport: game.Viewport2D,
	) {
		graphics_array := graphics_ptr[:length]
		entitiy_array := entity_ptr[:length]
		for &graphics_union, i in graphics_array {
			position := entitiy_array[i].position
			switch graphics in graphics_union.variant {
			case GraphicsNone:
			case GraphicsRect:
				{
					s := rl.Vector2{graphics.width, graphics.height}
					p := game.rect_to_viewport(viewport, position, s)
					rl.DrawRectangleV(p, s * viewport.scale, graphics.color)
				}
			}
		}
	}

	draw_entities_internal(
		len(world.players),
		world.players.graphics,
		world.players.entity,
		viewport,
	)
}

new_entity :: proc(world: ^World, $T: typeid, entity: T) -> (u32, bool) {
	entity := entity
	@(static)
	id: u32 = 0
	id += 1
	entity.id = id

	switch typeid_of(T) {
	case typeid_of(Player):
		{
			append_soa(&world.players, entity)
			return id, true
		}
	case:
		{
			id -= 1
			return id, false
		}
	}
}

main :: proc() {
	viewport := game.Viewport2D {
		width  = 1280,
		height = 720,
		scale  = 1,
	}

	world := World{}

	player_id, _ := new_entity(
		&world,
		Player,
		Player {
			position = {0, GROUND_LEVEL},
			movement_speed = 200,
			jump_speed = 700,
			grounded = true,
			variant = GraphicsRect{width = 50, height = 50, color = rl.BLUE},
		},
	)

	rl.InitWindow(
		auto_cast viewport.width,
		auto_cast viewport.height,
		"Simple Platformer",
	)
	rl.SetTargetFPS(60)

	for !rl.WindowShouldClose() {
		if rl.IsKeyPressed(rl.KeyboardKey.ESCAPE) {
			break
		}
		world_update(&world)
		world_draw(viewport, &world)
		rl.BeginDrawing()
		rl.ClearBackground(rl.BLACK)
		rl.EndDrawing()
	}
}
