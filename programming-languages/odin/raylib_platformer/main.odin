package main

import "core:fmt"
import "core:math"
import "game"
import rl "vendor:raylib"

GROUND_LEVEL :: -20
GRAVITY :: 2000
TERMINA_VELOCITY :: 500

World :: struct {
	debug_draw:     bool,
	players:        #soa[dynamic]Player,
	platforms:      #soa[dynamic]Platform,
	debug_graphics: #soa[dynamic]DebugGraphics,
}

DebugGraphics :: struct {
	position:       Position,
	using graphics: Graphics,
}

Entity :: struct {
	id: u32,
}

Position :: struct {
	using value: rl.Vector2,
}

Graphics :: struct {
	variant: union {
		GraphicsNone,
		GraphicsRect,
		GraphicsWireRect,
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
	using position: Position,
	velocity:       rl.Vector2,
	movement_speed: f32,
	jump_speed:     f32,
	grounded:       bool,
	collider:       Collider,
	animations:     PlayerAnimations,
}

Collider :: struct {
	variant: union {
		RectCollider,
	},
}

RectCollider :: struct {
	using center:  rl.Vector2,
	width, height: f32,
}

rect_center_to_leftcorner :: proc(x, y, width, height: f32) -> (f32, f32) {
	return (x - width / 2), (y - height / 2)
}
rect_leftcorner_to_center :: proc(x, y, width, height: f32) -> (f32, f32) {
	return (x + width / 2), (y + height / 2)
}

collider_get_raylib_rect :: proc(
	pos: rl.Vector2,
	rect_col: RectCollider,
) -> rl.Rectangle {
	x, y := rect_center_to_leftcorner(
		pos.x - rect_col.center.x,
		pos.y - rect_col.center.y,
		rect_col.width,
		rect_col.height,
	)
	return rl.Rectangle {
		width = rect_col.width,
		height = rect_col.height,
		x = x,
		y = y,
	}
}

Platform :: struct {
	using entity:   Entity,
	using graphics: Graphics,
	position:       Position,
	collider:       Collider,
}

GraphicsNone :: struct {}

GraphicsRect :: struct {
	width:  f32,
	height: f32,
	color:  rl.Color,
}

GraphicsWireRect :: struct {
	width:          f32,
	height:         f32,
	color:          rl.Color,
	line_thickness: f32,
}

player_update :: proc(
	players: #soa[]Player,
	platforms: #soa[]Platform,
	debug_graphics: ^#soa[dynamic]DebugGraphics,
) {
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
			player.velocity.y -= GRAVITY * dt
			player.position.value += player.velocity * dt
			player.velocity.y = math.max(player.velocity.y, -TERMINA_VELOCITY)
			// position.y = math.max(position.y, GROUND_LEVEL)
			player.grounded = false

			player_rect_collider := player.collider.variant.(RectCollider)
			for platform in platforms {
				intersect := rl.GetCollisionRec(
					collider_get_raylib_rect(
						player.position,
						player_rect_collider,
					),
					collider_get_raylib_rect(
						platform.position,
						platform.collider.variant.(RectCollider),
					),
				)
				if intersect.height != 0 {
					cx, cy := rect_leftcorner_to_center(
						intersect.x,
						intersect.y,
						intersect.width,
						intersect.height,
					)
					append_soa(
						debug_graphics,
						DebugGraphics {
							position = {value = {cx, cy}},
							graphics = {
								GraphicsWireRect {
									width = intersect.width,
									height = intersect.height,
									line_thickness = 3,
									color = rl.MAGENTA,
								},
							},
						},
					)
					player.grounded = true
					player.position.y += intersect.height
				}
			}
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
	player_update(world.players[:], world.platforms[:], &world.debug_graphics)
	animation_update(world.players.graphics[:len(world.players)])
}

world_draw :: proc(viewport: game.Viewport2D, world: ^World) {
	draw_entities(
		len(world.platforms),
		world.platforms.graphics,
		world.platforms.position,
		viewport,
	)

	draw_entities(
		len(world.players),
		world.players.graphics,
		world.players.position,
		viewport,
	)

	if world.debug_draw {
		debug_draw_colliders(
			viewport,
			len(world.players),
			world.players.collider,
			world.players.position,
		)

		debug_draw_colliders(
			viewport,
			len(world.platforms),
			world.platforms.collider,
			world.platforms.position,
		)

		draw_entities(
			len(world.debug_graphics),
			world.debug_graphics.graphics,
			world.debug_graphics.position,
			viewport,
		)
		clear_soa(&world.debug_graphics)

		rl.DrawFPS(10, 10)
	}
}

graphics_draw_rect_wired :: proc(
	viewport: game.Viewport2D,
	position: rl.Vector2,
	graphics: GraphicsWireRect,
) {
	s := rl.Vector2{graphics.width, graphics.height}
	p := game.rect_to_viewport(viewport, position, s)
	rl.DrawRectangleLinesEx(
		rl.Rectangle {
			x = p.x,
			y = p.y,
			width = s.x * viewport.scale,
			height = s.y * viewport.scale,
		},
		graphics.line_thickness,
		graphics.color,
	)
}

draw_entities :: proc(
	length: int,
	graphics: [^]Graphics,
	positions: [^]Position,
	viewport: game.Viewport2D,
) {
	for i := 0; i < length; i += 1 {
		position := positions[i].value
		switch graphics in graphics[i].variant {
		case GraphicsNone:
		case GraphicsRect:
			{
				s := rl.Vector2{graphics.width, graphics.height}
				p := game.rect_to_viewport(viewport, position, s)
				rl.DrawRectangleV(p, s * viewport.scale, graphics.color)
			}
		case GraphicsWireRect:
			graphics_draw_rect_wired(viewport, position, graphics)
		case game.SpriteAnimation:
			{
				rect := game.sprite_sheet_get_rect(
					graphics.sprite_sheet,
					graphics.current,
				)
				p := game.rect_to_viewport(viewport, position, graphics.size)
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

debug_draw_colliders :: proc(
	viewport: game.Viewport2D,
	len: int,
	colliders: [^]Collider,
	positions: [^]Position,
) {
	for i := 0; i < len; i += 1 {
		pos := positions[i].value
		collider := colliders[i]
		switch col in collider.variant {
		case RectCollider:
			{
				graphics_draw_rect_wired(
					viewport,
					pos - col.center,
					GraphicsWireRect {
						width = col.width,
						height = col.height,
						color = rl.GREEN,
						line_thickness = 1,
					},
				)
			}
		}
	}
}

get_entity_id :: proc() -> u32 {
	@(static)
	id: u32 = 0
	id += 1
	return id
}

create_player :: proc() -> Player {
	PLAYER_SIZE :: rl.Vector2{16 * 4, 16 * 4}
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
			size = PLAYER_SIZE,
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
		id = get_entity_id(),
		position = {GROUND_LEVEL},
		movement_speed = 200,
		jump_speed = 700,
		grounded = true,
		variant = player_animations.idle,
		animations = player_animations,
		collider = Collider {
			RectCollider {
				center = {0, 5},
				width = PLAYER_SIZE.x - 10,
				height = PLAYER_SIZE.y - 20,
			},
		},
	}
}

create_platform :: proc(
	pos: rl.Vector2 = {0, 0},
	width, height: f32,
) -> Platform {
	return Platform {
		id = get_entity_id(),
		position = {pos},
		variant = GraphicsRect{width = width, height = height, color = rl.RED},
		collider = Collider{RectCollider{width = width, height = height}},
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
	world.debug_graphics = make_soa(
		#soa[dynamic]DebugGraphics,
		context.temp_allocator,
	)

	defer free(world)

	append_soa(&world.players, create_player())
	append_soa(
		&world.platforms,
		create_platform(
			pos = {0, -viewport.height / 2 + 40},
			width = 150,
			height = 40,
		),
	)
	append_soa(
		&world.platforms,
		create_platform(
			pos = {200, -viewport.height / 2 + 40},
			width = 150,
			height = 40,
		),
	)
	append_soa(
		&world.platforms,
		create_platform(
			pos = {-200, -viewport.height / 2 + 40},
			width = 150,
			height = 40,
		),
	)

	for !rl.WindowShouldClose() {
		defer free_all(context.temp_allocator)

		//simulation
		{
			if rl.IsKeyPressed(rl.KeyboardKey.ESCAPE) {
				break
			}

			if rl.IsKeyPressed(rl.KeyboardKey.F11) {
				world.debug_draw = !world.debug_draw
			}
			world_update(world)
		}

		//render
		{
			rl.BeginDrawing()
			rl.ClearBackground(rl.BLACK)
			world_draw(viewport, world)
			rl.EndDrawing()
		}
	}
}
