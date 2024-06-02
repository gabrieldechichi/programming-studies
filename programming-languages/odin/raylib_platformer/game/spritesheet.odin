package game
import "../lib"
import "core:fmt"
import "core:strings"
import rl "vendor:raylib"

TextureHandle :: struct {
	texture:   rl.Texture2D `json:ignore`,
	file_path: lib.FixedString,
}

SpriteSheet :: struct {
	using handle: TextureHandle,
	rect_size:    rl.Vector2,
	row_count:    u32,
	column_count: u32,
}

sprite_sheet_new :: proc(
	file_path: string,
	row_count, column_count: u32,
) -> SpriteSheet {
	cfile_path := strings.clone_to_cstring(file_path, context.temp_allocator)
	tex := rl.LoadTexture(cfile_path)
	return SpriteSheet {
		texture = tex,
		file_path = lib.fixedstring_from_string_copy(file_path),
		row_count = row_count,
		column_count = column_count,
		rect_size = rl.Vector2 {
			auto_cast (u32(tex.width) / column_count),
			auto_cast (u32(tex.height) / row_count),
		},
	}
}

sprite_sheet_get_rect_row_column :: proc(
	ss: SpriteSheet,
	row, column: u32,
) -> rl.Rectangle {
	assert(row < ss.row_count)
	assert(column < ss.column_count)
	return rl.Rectangle {
		x = f32(column) * ss.rect_size.x,
		y = f32(row) * ss.rect_size.y,
		width = ss.rect_size.x,
		height = ss.rect_size.y,
	}
}

sprite_sheet_get_rect_index :: proc(
	ss: SpriteSheet,
	index: u32,
) -> rl.Rectangle {
	assert(index < ss.row_count * ss.column_count)
	row := index / ss.column_count
	column := index % ss.column_count
	return sprite_sheet_get_rect_row_column(ss, row, column)
}

sprite_sheet_get_rect :: proc {
	sprite_sheet_get_rect_index,
	sprite_sheet_get_rect_row_column,
}

SpriteAnimation :: struct {
	name:                u32,
	sprite_sheet:        SpriteSheet,
	size:                rl.Vector2,
	flip_x:              bool,
	fps:                 u8,
	timer:               f32,
	start, end, current: u32,
}
