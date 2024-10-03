package main

import "core:fmt"
import "core:math/linalg"
import gl "vendor:wasm/WebGL"

@(private = "file")
vertexSource :: `#version 300 es
  precision mediump float;

  layout(location=0) in vec2 aVertexPos;
  layout(location=1) in vec4 aTexCoord;
  layout(location=2) in vec4 aColor;
  layout(location=3) in mat4 aModelMatrix;

  uniform mat4 uViewProjectionMatrix;

  out vec2 vTexCoord;
  out vec4 vColor;

  void main() {
      vec4 pos = uViewProjectionMatrix * aModelMatrix * vec4(aVertexPos, 0.0, 1.0);

      gl_Position = pos;

      vTexCoord = (aVertexPos * vec2(0.5, -0.5) + 0.5) * aTexCoord.zw + aTexCoord.xy ;
      vColor = aColor;
  }
`

@(private = "file")
fragSource :: `#version 300 es
precision mediump float;

in vec2 vTexCoord;
in vec4 vColor;
out vec4 fragColor;

uniform sampler2D uSampler;

void main() {
    vec4 color = texture(uSampler, vTexCoord);
    if (color.a < 0.1){discard;}
    color *= vColor;
    fragColor = color;
}
`

QUAD :: []vec2{{-1, -1}, {1, -1}, {1, 1}, {1, 1}, {-1, 1}, {-1, -1}}

SpriteRenderer :: struct {
	program:               gl.Program,
	aVertexPos:            i32,
	aModelMatrix:          i32,
	aTexCoord:             i32,
	aColor:                i32,
	uSampler:              i32,
	uViewProjectionMatrix: i32,
	batchSize:             u32,
	batches:               [dynamic]SpriteRenderInstanceBatch,
}

SpriteRenderInstance :: struct {
	uvOffset:    vec2,
	uvSize:      vec2,
	color:       vec4,
	modelMatrix: mat4,
}

SpriteRenderInstanceBatch :: struct {
	texture:                gl.Texture,
	texWidth:               u32,
	texHeight:              u32,
	texIndex:               u32,
	buffer:                 gl.Buffer,
	vertexBuffer:           gl.Buffer,
	vao:                    gl.VertexArrayObject,
	instanceCountThisFrame: u32,
	instances:              []SpriteRenderInstance,
}

spriteRendererNew :: proc(inBatchSize: u32) -> (spriteRenderer: SpriteRenderer, err: WebGLError) {
	spriteRenderer = SpriteRenderer{}
	using spriteRenderer

	batchSize = inBatchSize
	batches = make([dynamic]SpriteRenderInstanceBatch, 0)
	program = glCreateProgramFromStrings(vertexSource, fragSource) or_return

	aVertexPos = gl.GetAttribLocation(program, "aVertexPos")
	aModelMatrix = gl.GetAttribLocation(program, "aModelMatrix")
	aTexCoord = gl.GetAttribLocation(program, "aTexCoord")
	aColor = gl.GetAttribLocation(program, "aColor")
	uSampler = gl.GetUniformLocation(program, "uSampler")
	uViewProjectionMatrix = gl.GetUniformLocation(program, "uViewProjectionMatrix")

	gl.Enable(gl.DEPTH_TEST)
	gl.DepthFunc(gl.LEQUAL)
	gl.Enable(gl.BLEND)
	gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)

	return spriteRenderer, .None
}


spriteRendererAddTexturePixels :: proc(
	spriteRenderer: ^SpriteRenderer,
	pixels: []byte,
	width: u32,
	height: u32,
) -> gl.Texture {
	using spriteRenderer
	//TODO: what if we go above the limit?
	nextIndex := u32(len(batches))
	webGLTex := createAndBindTextureFromPixels(
		texIndex = nextIndex,
		format = .RGBA,
		minFilter = .Nearest,
		magFilter = .Nearest,
		pixels = pixels,
		width = width,
		height = height,
	)

	spriteRendererAddInstanceBatch(spriteRenderer, webGLTex, width, height, nextIndex)

	return webGLTex
}

@(private = "file")
spriteRendererAddInstanceBatch :: proc(
	spriteRenderer: ^SpriteRenderer,
	webGLTex: gl.Texture,
	width: u32,
	height: u32,
	nextIndex: u32,
) {
	using spriteRenderer

	gl.UseProgram(program)
	//initialize buffers
	vao := gl.CreateVertexArray()
	gl.BindVertexArray(vao)

	//setup vertex buffer
	gl.EnableVertexAttribArray(aVertexPos)

	vertexBuffer := gl.CreateBuffer()
	gl.BindBuffer(gl.ARRAY_BUFFER, vertexBuffer)
	gl.BufferData(gl.ARRAY_BUFFER, size_of(QUAD[0]) * len(QUAD), raw_data(QUAD), gl.STATIC_DRAW)
	gl.VertexAttribPointer(
		aVertexPos,
		size = 2,
		type = gl.FLOAT,
		normalized = false,
		stride = size_of(vec2),
		ptr = 0,
	)

	//setup instance buffer
	instanceData := make([]SpriteRenderInstance, batchSize)

	buffer := gl.CreateBuffer()
	gl.BindBuffer(gl.ARRAY_BUFFER, buffer)

	gl.BufferData(
		target = gl.ARRAY_BUFFER,
		size = size_of(SpriteRenderInstance) * len(instanceData),
		data = raw_data(instanceData),
		usage = gl.DYNAMIC_DRAW,
	)

	gl.EnableVertexAttribArray(aTexCoord)
	gl.EnableVertexAttribArray(aColor)
	gl.VertexAttribDivisor(u32(aTexCoord), 1)
	gl.VertexAttribDivisor(u32(aColor), 1)


	gl.VertexAttribPointer(
		aTexCoord,
		size = 4,
		type = gl.FLOAT,
		normalized = false,
		stride = size_of(SpriteRenderInstance),
		ptr = offset_of(SpriteRenderInstance, uvOffset),
	)

	gl.VertexAttribPointer(
		aColor,
		size = 4,
		type = gl.FLOAT,
		normalized = false,
		stride = size_of(SpriteRenderInstance),
		ptr = offset_of(SpriteRenderInstance, color),
	)


	for i in i32(0) ..< 4 {
		gl.EnableVertexAttribArray(aModelMatrix + i)

		// Set the vertex attribute parameters
		gl.VertexAttribPointer(
			aModelMatrix + i32(i),
			size = 4,
			type = gl.FLOAT,
			normalized = false,
			stride = size_of(SpriteRenderInstance),
			ptr = uintptr(
				uint(offset_of(SpriteRenderInstance, modelMatrix)) + (uint(i) * size_of(vec4)),
			),
		)

		gl.VertexAttribDivisor(u32(aModelMatrix + i), 1)
	}

	gl.BindVertexArray(0)

	newBatch := SpriteRenderInstanceBatch {
		texture                = webGLTex,
		texWidth               = width,
		texHeight              = height,
		texIndex               = nextIndex,
		buffer                 = buffer,
		instances              = instanceData,
		vao                    = vao,
		vertexBuffer           = vertexBuffer,
		instanceCountThisFrame = 0,
	}

	append_elem(&batches, newBatch)
}

spriteRendererDrawSprite :: proc(
	spriteRenderer: ^SpriteRenderer,
	sprite: Sprite,
	pos: vec3 = {0, 0, 0},
	scale: vec2 = {0, 0},
	color: vec4 = {1, 1, 1, 1},
	pivot: Pivot = .Center,
) -> bool {
	using spriteRenderer
	index := -1
	for batch, i in batches {
		if batch.texture == sprite.texture {
			index = i
			break
		}
	}
	if index < 0 {
		return false
	}

	batch := &batches[index]

	uvOffset, uvSize := getUvOffsetAndScale(sprite, batch.texWidth, batch.texHeight)

	pos := pos
	switch (pivot) {
	case .Center:
	case .TopLeft:
		pos.x += scale.x
		pos.y -= scale.y
	case .BottomLeft:
		pos.x += scale.x
		pos.y += scale.y
	}

	modelMatrix := linalg.matrix4_from_trs(
		pos,
		linalg.quaternion_from_euler_angle_z(f32(0)),
		vec3{scale.x, scale.y, 1},
	)

	instanceData := SpriteRenderInstance {
		uvOffset    = uvOffset,
		uvSize      = uvSize,
		color       = color,
		modelMatrix = modelMatrix,
	}
	batch.instances[batch.instanceCountThisFrame] = instanceData

	//todo: guard against MAX_INSTANCE
	batch.instanceCountThisFrame += 1

	return true
}

spriteRendererRender :: proc(spriteRenderer: ^SpriteRenderer, viewProjectionMatrix: mat4) {
	using spriteRenderer

	gl.ClearColor(0.0, 0.0, 0.0, 1.0)
	gl.Clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT)

	gl.UseProgram(program)
	gl.UniformMatrix4fv(uViewProjectionMatrix, viewProjectionMatrix)

	for batch in batches {
		using batch
		if instanceCountThisFrame <= 0 {
			continue
		}

		gl.BindVertexArray(vao)
		gl.Uniform1i(uSampler, auto_cast texIndex)
		gl.BindBuffer(gl.ARRAY_BUFFER, buffer)
		gl.BufferData(
			gl.ARRAY_BUFFER,
			len(instances) * size_of(SpriteRenderInstance),
			raw_data(instances),
			gl.DYNAMIC_DRAW,
		)

		gl.DrawArraysInstanced(gl.TRIANGLES, 0, 6, auto_cast instanceCountThisFrame)
	}

	//end frame
	for &batch in batches {
		batch.instanceCountThisFrame = 0
	}
}
