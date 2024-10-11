package main

import "core:encoding/xml"
import "core:fmt"
import "core:sys/wasm/js"
import gl "vendor:wasm/WebGL"

WebGLError :: enum {
	None,
	FailedToCreateProgram,
}

TextureFormat :: enum gl.Enum {
	RGB  = gl.RGB,
	RGBA = gl.RGBA,
}

TexFiltering :: enum gl.Enum {
	Nearest = gl.NEAREST,
	Linear  = gl.LINEAR,
}

SpriteRegion :: struct {
	x, y, w, h: f32,
}

Sprite :: struct {
	using region: SpriteRegion,
	texture:      gl.Texture,
}

Pivot :: enum {
	Center = 0,
	TopLeft,
	BottomLeft,
}

glCreateProgramFromStrings :: proc(vert: string, frag: string) -> (gl.Program, WebGLError) {
	program, ok := gl.CreateProgramFromStrings([]string{vert}, []string{frag})
	if !ok {
		return program, .FailedToCreateProgram
	}
	return program, .None
}

createAndBindTextureFromPixels :: proc(
	pixels: []byte,
	width: u32,
	height: u32,
	texIndex: u32,
	format: TextureFormat,
	minFilter: TexFiltering,
	magFilter: TexFiltering,
	generateMipMaps: bool = false,
	flipY: bool = false,
) -> gl.Texture {
	gl.ActiveTexture(gl.Enum(u32(gl.TEXTURE0) + texIndex))
	texture := gl.CreateTexture()
	gl.BindTexture(gl.TEXTURE_2D, texture)
	gl.PixelStorei(gl.UNPACK_FLIP_Y_WEBGL, i32(flipY))

	gl.TexImage2D(
		target = gl.TEXTURE_2D,
		level = 0,
		internalformat = gl.Enum(format),
		width = i32(width),
		height = i32(height),
		border = 0,
		format = gl.Enum(format),
		type = gl.UNSIGNED_BYTE,
		size = len(pixels),
		data = raw_data(pixels),
	)

	if generateMipMaps {
		gl.GenerateMipmap(gl.TEXTURE_2D)
	}

	gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, i32(minFilter))
	gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, i32(magFilter))

	return texture
}

getUvOffsetAndScale :: proc(
	sprite: SpriteRegion,
	texWidth: u32,
	texHeight: u32,
	padding: u32 = 0,
) -> (
	offset: vec2,
	size: vec2,
) {
	u := sprite.x / f32(texWidth)
	v := sprite.y / f32(texHeight)
	w := sprite.w / f32(texWidth)
	h := sprite.h / f32(texHeight)

	hPadding := f32(padding) / f32(texWidth)
	vPadding := f32(padding) / f32(texHeight)

	offset = vec2{u + hPadding, v + vPadding}
	size = vec2{w - hPadding, h - vPadding}
	return
}


FontChar :: struct {
	id:      u32,
	xy:      vec2,
	wh:      vec2,
	offset:  vec2,
	advance: f32,
}

Font :: struct {
	texture:    gl.Texture,
	lineHeight: f32,
	size:       f32,
	characters: map[u32]FontChar,
}

charToSprite :: proc(c: u32, font: Font) -> Sprite {
	fontChar := font.characters[c]

	return Sprite {
		texture = font.texture,
		x = fontChar.xy[0],
		y = fontChar.xy[1],
		w = fontChar.wh[0],
		h = fontChar.wh[1],
	}
}
