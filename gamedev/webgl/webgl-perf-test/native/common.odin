package main

import "core:fmt"
import gl "vendor:wasm/WebGL"

GraphicsError :: enum {
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

glCreateProgramFromStrings :: proc(vert: string, frag: string) -> (gl.Program, GraphicsError) {
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
