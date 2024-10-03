package main

import gl "vendor:wasm/WebGL"

GraphicsError :: enum {
	None,
	FailedToCreateProgram,
}

glCreateProgramFromStrings :: proc(vert: string, frag: string) -> (gl.Program, GraphicsError) {
	program, ok := gl.CreateProgramFromStrings(
		[]string{vert},
		[]string{frag},
	)
	if !ok {
		return program, .FailedToCreateProgram
	}
	return program, .None
}
