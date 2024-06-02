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
		game.SpriteAnimation,
	},
}

PlayerAnimationName :: enum u32 {
	Idle = 0,
	Run  = 1,
}

PlayerAnimations :: struct {
	idle: game.SpriteAnimation,
	run:  game.SpriteAnimation,
}

Player :: struct {
	using graphics: Graphics,
	using entity:   Entity,
	velocity:       rl.Vector2,
	movement_speed: f32,
	jump_speed:     f32,
	grounded:       bool,
	animations:     PlayerAnimations,
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

		//graphics
		{
			using player.graphics
			using PlayerAnimationName

			#partial switch &graphics in variant {
			case game.SpriteAnimation:
				flip_x, is_moving := flip_x.?
				if is_moving {
					graphics.flip_x = flip_x
				}
				flip_x = graphics.flip_x
				if is_moving && graphics.name != auto_cast Run {
					player.graphics.variant = player.animations.run
					graphics.current = graphics.start
				} else if !is_moving && graphics.name != auto_cast Idle {
					player.graphics.variant = player.animations.idle
					graphics.current = graphics.start
				}
				graphics.flip_x = flip_x
			}
		}
	}
}

animation_update :: proc(animations: []Graphics) {
	dt := rl.GetFrameTime()
	for &gu in animations {
		#partial switch &anim in gu.variant {
		case game.SpriteAnimation:
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
			case game.SpriteAnimation:
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

create_player :: proc() -> Player {
	create_anim :: proc(
		name: PlayerAnimationName,
		path: string,
		rows, columns: u32,
		start: u32 = 0,
		end: u32 = 0,
		fps: u8 = 12,
	) -> game.SpriteAnimation {
			end := end
		if end == 0 {
			end = rows * columns - 1
		}
		return game.SpriteAnimation {
			name = auto_cast name,
			sprite_sheet = game.sprite_sheet_new_path(
				path,
				row_count = rows,
				column_count = columns,
			),
			size = rl.Vector2{50, 50},
			fps = fps,
			start = start,
			end = end,
			current = start,
		}
	}

	using PlayerAnimationName

	player_animations := PlayerAnimations {
		idle = create_anim(
			Idle,
			"./assets/animations/cat_idle.png",
			rows = 1,
			columns = 2,
			fps = 3,
		),
		run  = create_anim(
			Run,
			"./assets/animations/cat_run.png",
			rows = 1,
			columns = 4,
		),
	}

	return Player {
		position = {0, GROUND_LEVEL},
		movement_speed = 200,
		jump_speed = 700,
		grounded = true,
		variant = player_animations.idle,
		animations = player_animations,
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

	new_entity(world, Player, create_player())

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
