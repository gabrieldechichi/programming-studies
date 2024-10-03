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
      pos = uViewProjectionMatrix * aModelMatrix * vec4(aVertexPos, 0.0, 1.0);

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
    // if (color.a < 0.1){fragColor = vec4(1,0,0,1); return;}
    fragColor = color;
}
`

QUAD :: []vec2{}

SpriteRenderer :: struct {
	program:               gl.Program,
	aVertexPos:            i32,
	aModelMatrix:          i32,
	aTexCoord:             i32,
	aColor:                i32,
	uSampler:              i32,
	uViewProjectionMatrix: i32,
	batchSize:             u32,
	batches:               []SpriteRenderInstanceBatch,
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

newSpriteRenderer :: proc(
	inBatchSize: u32,
) -> (
	spriteRenderer: SpriteRenderer,
	err: GraphicsError,
) {
	spriteRenderer = SpriteRenderer{}
	using spriteRenderer

	batchSize = inBatchSize
	batches = make([]SpriteRenderInstanceBatch, batchSize)
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
