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
		SpriteAnimation,
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

SpriteAnimation :: struct {
	sprite_sheet:        game.SpriteSheet,
	size:                rl.Vector2,
	flip_x:              bool,
	fps:                 u8,
	timer:               f32,
	start, end, current: u32,
}

player_update :: proc(players: #soa[]Player) {
	dt := rl.GetFrameTime()
	for &player in players {
		flip_x: Maybe(bool)
		//input
		{
			player.velocity.x = 0
			if rl.IsKeyDown(rl.KeyboardKey.A) {
				player.velocity.x = -player.movement_speed
				flip_x = true
			}
			if rl.IsKeyDown(rl.KeyboardKey.D) {
				player.velocity.x = player.movement_speed
				flip_x = false
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

		if f, ok := flip_x.?; ok {
			#partial switch &graphics in player.graphics.variant {
			case SpriteAnimation:
				graphics.flip_x = f
			}
		}
	}
}

animation_update :: proc(animations: []Graphics) {
	dt := rl.GetFrameTime()
	for &gu in animations {
		#partial switch &anim in gu.variant {
		case SpriteAnimation:
			{
				anim.timer += dt
				if anim.timer > 1.0 / f32(anim.fps) {
					anim.timer = 0
					anim.current += 1
					if anim.current > anim.end {
						anim.current = anim.start
					}
				}
			}
		}
	}
}

world_update :: proc(world: ^World) {
	player_update(world.players[:])
	animation_update(world.players.graphics[:len(world.players)])
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
			case SpriteAnimation:
				{
					rect := game.sprite_sheet_get_rect(
						graphics.sprite_sheet,
						graphics.current,
					)
					p := game.rect_to_viewport(
						viewport,
						position,
						graphics.size,
					)
					if graphics.flip_x {rect.width *= -1}
					rl.DrawTexturePro(
						graphics.sprite_sheet.texture,
						rect,
						rl.Rectangle {
							x = p.x,
							y = p.y,
							width = graphics.size.x,
							height = graphics.size.y,
						},
						0,
						0,
						rl.WHITE,
					)
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

	rl.InitWindow(
		auto_cast viewport.width,
		auto_cast viewport.height,
		"Simple Platformer",
	)
	rl.SetTargetFPS(60)

	world := new(World)
	defer free(world)

	player_id, _ := new_entity(
		world,
		Player,
		Player {
			position = {0, GROUND_LEVEL},
			movement_speed = 200,
			jump_speed = 700,
			grounded = true,
			variant = SpriteAnimation {
				sprite_sheet = game.sprite_sheet_new(
					"./assets/animations/cat_run.png",
					row_count = 1,
					column_count = 4,
				),
				size = rl.Vector2{50, 50},
				fps = 12,
				start = 0,
				end = 3,
				current = 0,
			},
		},
	)


	for !rl.WindowShouldClose() {
		if rl.IsKeyPressed(rl.KeyboardKey.ESCAPE) {
			break
		}
		world_update(world)
		world_draw(viewport, world)
		rl.BeginDrawing()
		rl.ClearBackground(rl.BLACK)
		rl.EndDrawing()
		free_all(context.temp_allocator)
	}
}
