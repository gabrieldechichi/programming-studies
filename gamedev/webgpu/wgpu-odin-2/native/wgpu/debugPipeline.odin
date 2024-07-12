package wgpuimpl

import "core:math/linalg"
import "vendor:wgpu"

mat4x4 :: linalg.Matrix4x4f32
vec2 :: linalg.Vector2f32

@(private = "file")
shader :: `
struct VertexIn {
    @builtin(instance_index) instanceIdx: u32,
    @location(0) pos: vec2f,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) col: vec4f,
}

struct Globals {
    viewProjectionMatrix: mat4x4f,
    colors: array<vec4f, 4>,
}

@group(0) @binding(0) var<uniform> globals: Globals;
@group(1) @binding(0) var<uniform> modelMatrices: array<mat4x4f, 1024>;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    let modelMatrix: mat4x4f = modelMatrices[in.instanceIdx];
    out.pos = globals.viewProjectionMatrix * modelMatrix * vec4(in.pos, 0.0, 1.0);
    out.col = globals.colors[in.instanceIdx % 4];
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    return in.col;
}
`

DebugPipelineVertexIn :: struct {
	pos: vec2,
}

DebugPipelineCreateParams :: struct {
	device:        wgpu.Device,
	textureFormat: wgpu.TextureFormat,
}

DebugPipelineGlobalUniforms :: struct {
	viewProjectionMatrix: mat4x4,
	colors:               []wgpu.Color,
}

DebugPipelineModelMatrixUniforms :: struct {}

DebugPipeline :: struct {
	pipeline:                    wgpu.RenderPipeline,
	globalUniformsGroupLayout:   wgpu.BindGroupLayout,
	instanceUniformsGroupLayout: wgpu.BindGroupLayout,
}


debugPipelineCreate :: proc(
	using createParams: DebugPipelineCreateParams,
) -> DebugPipeline {
	pipeline := DebugPipeline{}

	pipeline.globalUniformsGroupLayout = wgpu.DeviceCreateBindGroupLayout(
		device,
		&wgpu.BindGroupLayoutDescriptor {
			entryCount = 1,
			entries = raw_data(
				[]wgpu.BindGroupLayoutEntry {
					{
						binding = 0,
						visibility = {wgpu.ShaderStage.Vertex},
						buffer = wgpu.BufferBindingLayout {
							type = wgpu.BufferBindingType.Uniform,
						},
					},
				},
			),
		},
	)

	pipeline.instanceUniformsGroupLayout = wgpu.DeviceCreateBindGroupLayout(
		device,
		&wgpu.BindGroupLayoutDescriptor {
			entryCount = 1,
			entries = raw_data(
				[]wgpu.BindGroupLayoutEntry {
					{
						binding = 0,
						visibility = {wgpu.ShaderStage.Vertex},
						buffer = wgpu.BufferBindingLayout {
							type = wgpu.BufferBindingType.Uniform,
						},
					},
				},
			),
		},
	)

	module := wgpu.DeviceCreateShaderModule(
		device,
		&wgpu.ShaderModuleDescriptor {
			nextInChain = &wgpu.ShaderModuleWGSLDescriptor {
				sType = wgpu.SType.ShaderModuleWGSLDescriptor,
				code = shader,
			},
		},
	)

	vertex := wgpu.VertexState {
		module      = module,
		entryPoint  = "vertexMain",
		bufferCount = 1,
		buffers     = raw_data(
			[]wgpu.VertexBufferLayout {
				{
					arrayStride = size_of(DebugPipelineVertexIn),
					stepMode = wgpu.VertexStepMode.Vertex,
					attributeCount = 1,
					attributes = raw_data(
						[]wgpu.VertexAttribute {
							{
								format = wgpu.VertexFormat.Float32x2,
								offset = 0,
								shaderLocation = 0,
							},
						},
					),
				},
			},
		),
	}

	fragment := wgpu.FragmentState {
		module      = module,
		entryPoint  = "fragmentMain",
		targetCount = 1,
		targets     = raw_data(
			[]wgpu.ColorTargetState {
				{
					format = textureFormat,
					blend = &wgpu.BlendState {
						color = wgpu.BlendComponent {
							operation = wgpu.BlendOperation.Add,
							srcFactor = wgpu.BlendFactor.SrcAlpha,
							dstFactor = wgpu.BlendFactor.OneMinusSrcAlpha,
						},
						alpha = wgpu.BlendComponent {
							operation = wgpu.BlendOperation.Add,
							srcFactor = wgpu.BlendFactor.One,
							dstFactor = wgpu.BlendFactor.OneMinusSrcAlpha,
						},
					},
				},
			},
		),
	}

	pipeline.pipeline = wgpu.DeviceCreateRenderPipeline(
		device,
		&wgpu.RenderPipelineDescriptor {
			label = "Debug Pipeline",
			vertex = vertex,
			fragment = &fragment,
			layout = wgpu.DeviceCreatePipelineLayout(
				device,
				&wgpu.PipelineLayoutDescriptor {
					bindGroupLayoutCount = 1,
					bindGroupLayouts = raw_data(
						[]wgpu.BindGroupLayout {
							pipeline.globalUniformsGroupLayout,
							pipeline.instanceUniformsGroupLayout,
						},
					),
				},
			),
			primitive = wgpu.PrimitiveState {
				topology = wgpu.PrimitiveTopology.TriangleList,
				stripIndexFormat = wgpu.IndexFormat.Uint16,
				frontFace = wgpu.FrontFace.CCW,
				cullMode = wgpu.CullMode.Back,
			},
		},
	)
	return pipeline
}