package main

import "base:runtime"
import "core:fmt"
import "core:math"
import "core:mem"
import "core:os"
import "core:reflect"
import "core:strings"
import "game"
import sprites "game/sprites"
import "lib/fixed_string"
import rl "vendor:raylib"

GROUND_LEVEL :: -20
GRAVITY :: 2000
TERMINA_VELOCITY :: 500

World :: struct {
	debug_draw:     bool,
	debug_graphics: #soa[dynamic]DebugGraphics,
	players:        #soa[dynamic]Player,
	platforms:      #soa[dynamic]Platform,
}

Stuff :: struct {
	value: u8,
}

DebugGraphics :: struct {
	position: Position,
	graphics: union {
		GraphicsRect,
		GraphicsWireRect,
	},
}

Entity :: struct {
	id: u32,
}

Position :: struct {
	using value: rl.Vector2,
}

PlayerAnimationName :: enum u32 {
	None = 0,
	Idle = 1,
	Run  = 2,
}

PlayerAnimations :: struct {
	idle: sprites.SpriteAnimation,
	run:  sprites.SpriteAnimation,
}

Player :: struct {
	using entity:   Entity,
	using position: Position,
	graphics:       sprites.SpriteAnimation,
	velocity:       rl.Vector2,
	movement_speed: f32,
	jump_speed:     f32,
	grounded:       bool,
	collider:       Collider2D,
	animations:     PlayerAnimations,
}

Collider2D :: struct {
	variant: union {
		RectCollider,
	},
}

RectCollider :: struct {
	using center:  rl.Vector2,
	width, height: f32,
}

Platform :: struct {
	using entity: Entity,
	graphics:     sprites.SpriteAnimation,
	position:     Position,
	collider:     Collider2D,
}

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
							graphics = GraphicsWireRect {
								width = intersect.width,
								height = intersect.height,
								line_thickness = 3,
								color = rl.MAGENTA,
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
			using PlayerAnimationName

			flip_x, is_moving := flip_x.?
			if is_moving {
				player.graphics.flip_x = flip_x
			}
			flip_x = player.graphics.flip_x
			if is_moving && player.graphics.name != auto_cast Run {
				player.graphics = player.animations.run
				player.graphics.current = player.graphics.start
			} else if !is_moving && player.graphics.name != auto_cast Idle {
				player.graphics = player.animations.idle
				player.graphics.current = player.graphics.start
			}
			player.graphics.flip_x = flip_x
		}
	}
}

animation_update :: proc(animations: []sprites.SpriteAnimation) {
	dt := rl.GetFrameTime()
	for &anim in animations {
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

world_update :: proc(world: ^World) {
	player_update(world.players[:], world.platforms[:], &world.debug_graphics)
	animation_update(world.players.graphics[:len(world.players)])
}

world_draw :: proc(viewport: game.Viewport2D, world: ^World) {
	for player in world.players {
		graphics_draw_sprite_animation(
			player.graphics,
			player.position,
			viewport,
		)
	}

	for platform in world.platforms {
		graphics_draw_sprite_animation(
			platform.graphics,
			platform.position,
			viewport,
		)
	}

	when ODIN_DEBUG {
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

			debug_draw_graphics(viewport, world.debug_graphics[:])

			rl.DrawFPS(10, 10)
		}
		clear_soa(&world.debug_graphics)
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

graphics_draw_sprite_animation :: proc(
	graphics: sprites.SpriteAnimation,
	position: Position,
	viewport: game.Viewport2D,
) {
	rect := sprites.sprite_sheet_get_rect(graphics.sprite_sheet, graphics.current)
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

debug_draw_graphics :: proc(
	viewport: game.Viewport2D,
	debug_graphics: #soa[]DebugGraphics,
) {
	for dg in debug_graphics {
		#partial switch graphics in dg.graphics {
		case GraphicsWireRect:
			graphics_draw_rect_wired(viewport, dg.position, graphics)
		}
	}
}

debug_draw_colliders :: proc(
	viewport: game.Viewport2D,
	len: int,
	colliders: [^]Collider2D,
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

world_new :: proc() -> ^World {
	world := new(World)
	return world
}

world_delete :: proc(world: ^World) {
	delete_soa(world.players)
	delete_soa(world.platforms)
	delete_soa(world.debug_graphics)
	free(world)
}

SnapshotSOA :: struct {
	type:   typeid,
	length: int,
	data:   []byte,
}

serialize_soa :: proc(
	arr: #soa[]$T,
	allocator: mem.Allocator = context.allocator,
) -> []byte {
	arr_length := len(arr)
	bytes := make([dynamic]byte, 0, 0, allocator)
	append(&bytes, ..mem.ptr_to_bytes(&arr_length))

	id := typeid_of(type_of(arr))
	field_names := reflect.struct_field_names(id)
	for field_name, i in field_names {
		if field_name == "__$len" do continue
		field := reflect.struct_field_at(id, i)
		value := reflect.struct_field_value(arr, field)

		#partial switch type_var in field.type.variant {
		case runtime.Type_Info_Multi_Pointer:
			{
				ptr := ((^rawptr)(value.data))^
				value_bytes := transmute([]byte)runtime.Raw_Slice {
					ptr,
					type_var.elem.size * arr_length,
				}
				append(&bytes, ..value_bytes)
			}
		case:
			fmt.println(
				"Unsupported type ",
				field.type.variant,
				"Field ",
				field_name,
			)
		}
	}
	return bytes[:]
}

deserialize_soa :: proc(
	$T: typeid/#soa[dynamic]$E,
	bytes: []byte,
	allocator: mem.Allocator = context.allocator,
) -> T {
	cursor: int = 0
	arr_length := (int)(
		(^uintptr)(raw_data(bytes[cursor:cursor + size_of(int)]))^,
	)
	cursor += size_of(int)
	soa := make_soa(T, arr_length, arr_length)

	id := typeid_of(type_of(soa))
	field_names := reflect.struct_field_names(id)
	for field_name, i in field_names {
		if field_name == "__$len" do continue

		field := reflect.struct_field_at(id, i)
		value := reflect.struct_field_value(soa, field)

		#partial switch type_var in field.type.variant {
		case runtime.Type_Info_Multi_Pointer:
			{
				span := type_var.elem.size * arr_length
				value_bytes := bytes[cursor:cursor + span]
				value_ptr := ((^rawptr)(value.data))^
				mem.copy(value_ptr, raw_data(value_bytes), span)
				cursor += span
			}
		case:
			fmt.println(
				"Unsupported type ",
				field.type.variant,
				"Field ",
				field_name,
			)
		}
	}
	sbuilder := strings.builder_make(allocator = context.temp_allocator)
	assert(
		cursor == len(bytes),
		fmt.sbprintf(
			&sbuilder,
			"Mismatch cursor(%d), bytes len (%d)",
			cursor,
			len(bytes),
		),
	)
	return soa
}

world_serialize :: proc(
	world: World,
	allocator: mem.Allocator = context.allocator,
) -> []byte {
	buffer := make([dynamic]byte, 0, 0, allocator)
	platform_bytes := serialize_soa(world.platforms[:], allocator)
	defer delete(platform_bytes, allocator)
	append(&buffer, ..platform_bytes[:])


	return buffer[:]
}

world_deserialize :: proc(world: ^World, bytes: []byte) {
	// delete_soa(world.platforms)
	// world.platforms = mem.reinterpret_copy(
	// 	#soa[dynamic]Platform,
	// 	raw_data(bytes),
	// )

	delete_soa(world.platforms)
	world.platforms = deserialize_soa(#soa[dynamic]Platform, bytes)
	for &platform in world.platforms {
		platform.graphics.sprite_sheet = sprites.sprite_sheet_new(
			fixed_string.to_string_64(
				platform.graphics.sprite_sheet.file_path,
				context.temp_allocator,
			),
			row_count = 1,
			column_count = 1,
		)
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
	) -> sprites.SpriteAnimation {
		end := end
		if end == 0 {
			end = rows * columns - 1
		}
		return sprites.SpriteAnimation {
			name = auto_cast name,
			sprite_sheet = sprites.sprite_sheet_new(
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
		graphics = player_animations.idle,
		animations = player_animations,
		collider = Collider2D {
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
		graphics = sprites.SpriteAnimation {
			name = auto_cast PlayerAnimationName.None,
			sprite_sheet = sprites.sprite_sheet_new(
				"./assets/animations/platform.png",
				row_count = 1,
				column_count = 1,
			),
			size = {width, height},
			fps = 1,
		},
		collider = Collider2D{RectCollider{width = width, height = height}},
	}
}

main :: proc() {
	when ODIN_DEBUG {
		track: mem.Tracking_Allocator
		mem.tracking_allocator_init(&track, context.allocator)
		context.allocator = mem.tracking_allocator(&track)

		defer {
			if len(track.allocation_map) > 0 {
				fmt.eprintf(
					"=== %v allocations not freed: ===\n",
					len(track.allocation_map),
				)
				for _, entry in track.allocation_map {
					fmt.eprintf(
						"- %v bytes @ %v\n",
						entry.size,
						entry.location,
					)
				}
			}
			if len(track.bad_free_array) > 0 {
				fmt.eprintf(
					"=== %v incorrect frees: ===\n",
					len(track.bad_free_array),
				)
				for entry in track.bad_free_array {
					fmt.eprintf("- %p @ %v\n", entry.memory, entry.location)
				}
			}
			mem.tracking_allocator_destroy(&track)
		}
	}

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

	world := world_new()

	defer world_delete(world)

	append_soa(&world.players, create_player())

	load := true
	if load {

		if bytes, ok := os.read_entire_file_from_filename(
			"./world_dump.bytes",
			context.temp_allocator,
		); ok {
			world_deserialize(world, bytes)
		}
	} else {
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
	}

	for !rl.WindowShouldClose() {
		defer free_all(context.temp_allocator)

		//simulation
		{
			if rl.IsKeyPressed(rl.KeyboardKey.ESCAPE) {
				break
			}

			when ODIN_DEBUG {
				if rl.IsKeyPressed(rl.KeyboardKey.F11) {
					world.debug_draw = !world.debug_draw
				}
			} else {world.debug_draw = false}

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

	bytes := world_serialize(world^, context.temp_allocator)
	defer delete(bytes, context.temp_allocator)
	os.write_entire_file("./world_dump.bytes", bytes)
}
