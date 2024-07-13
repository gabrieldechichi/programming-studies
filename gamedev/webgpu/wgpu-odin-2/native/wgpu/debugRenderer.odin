package wgpuimpl

import "core:fmt"
import "lib:common"
import "vendor:wgpu"

DebugRenderer :: struct {
	pipeline:         DebugPipeline,
	vertexBuffer:     WgpuVertexBuffer,
	vertexCount:      u32,
	indexBuffer:      WgpuIndexBuffer,
	indexCount:       u32,
	globalsBuffer:    WgpuUniformBuffer,
	globalsBindGroup: wgpu.BindGroup,
	batches:          [dynamic]DebugRendererInstanceBatch,
	frameBatchCount:  u16,
}

DebugRendererInstanceBatch :: struct {
	modelMatricesBuffer: WgpuUniformBuffer,
	modelMatrices:       []common.mat4x4,
	bindGroup:           wgpu.BindGroup,
}

Mesh :: struct {
	vertices: []f32,
	indices:  []u16,
}

GEO_QUAD_CENTERED :: Mesh {
	vertices = []f32 {
		//pos
		-0.5,
		-0.5,
		0.5,
		-0.5,
		0.5,
		0.5,
		-0.5,
		0.5,
	},
	indices  = []u16{0, 1, 2, 0, 2, 3},
}

debugRendererInstanceBatchCreate :: proc(
	device: wgpu.Device,
	groupLayout: wgpu.BindGroupLayout,
) -> DebugRendererInstanceBatch {

	batch := DebugRendererInstanceBatch{}
	batch.modelMatricesBuffer = bufferCreateUniform(
		device,
		DebugPipelineModelMatrixUniforms,
		1,
	)
	batch.modelMatrices = make([]common.mat4x4, DEBUGPIPELINE_INSTANCE_COUNT)
	batch.bindGroup = wgpu.DeviceCreateBindGroup(
		device,
		&wgpu.BindGroupDescriptor {
			layout = groupLayout,
			entryCount = 1,
			entries = raw_data(
				[]wgpu.BindGroupEntry {
					{
						binding = 0,
						buffer = auto_cast batch.modelMatricesBuffer,
						offset = 0,
						size = wgpu.BufferGetSize(
							auto_cast batch.modelMatricesBuffer,
						),
					},
				},
			),
		},
	)
	return batch
}

debugRendererCreate :: proc(
	using params: DebugPipelineCreateParams,
) -> DebugRenderer {
	renderer := DebugRenderer {
		pipeline = debugPipelineCreate(params),
	}

	renderer.globalsBuffer = bufferCreateUniform(
		device,
		DebugPipelineGlobalUniforms,
		1,
	)

	renderer.globalsBindGroup = wgpu.DeviceCreateBindGroup(
		device,
		&wgpu.BindGroupDescriptor {
			layout = renderer.pipeline.globalUniformsGroupLayout,
			entryCount = 1,
			entries = raw_data(
				[]wgpu.BindGroupEntry {
					{
						binding = 0,
						buffer = auto_cast renderer.globalsBuffer,
						offset = 0,
						size = wgpu.BufferGetSize(
							auto_cast renderer.globalsBuffer,
						),
					},
				},
			),
		},
	)

	renderer.vertexBuffer = bufferCreateVertex(
		device,
		GEO_QUAD_CENTERED.vertices,
	)
	renderer.vertexCount = auto_cast len(GEO_QUAD_CENTERED.vertices)

	renderer.indexBuffer = bufferCreateIndex(device, GEO_QUAD_CENTERED.indices)
	renderer.indexCount = auto_cast len(GEO_QUAD_CENTERED.indices)

	return renderer
}

debugRendererRender :: proc(
	using renderer: DebugRenderer,
	passEncoder: wgpu.RenderPassEncoder,
	queue: wgpu.Queue,
	projectionViewMatrix: common.mat4x4,
) {
	if (len(batches) <= 0) {return}
	wgpu.RenderPassEncoderSetPipeline(passEncoder, pipeline.pipeline)
	wgpu.RenderPassEncoderSetVertexBuffer(
		passEncoder,
		slot = 0,
		buffer = auto_cast vertexBuffer,
		offset = 0,
		size = auto_cast vertexCount * size_of(f32),
	)

	wgpu.RenderPassEncoderSetIndexBuffer(
		passEncoder,
		buffer = auto_cast indexBuffer,
		format = wgpu.IndexFormat.Uint16,
		offset = 0,
		size = auto_cast indexCount * size_of(u16),
	)

	wgpu.RenderPassEncoderSetBindGroup(
		passEncoder,
		groupIndex = 0,
		group = globalsBindGroup,
	)

	globals := DebugPipelineGlobalUniforms {
		viewProjectionMatrix = projectionViewMatrix,
		colors               = [4]common.Color {
			{r = 1, g = 0.5, b = 0, a = 1},
			{r = 0.2, g = 1, b = 0.5, a = 1},
			{r = 0.2, g = 1, b = 0.4, a = 1},
			{r = 0.4, g = 0.1, b = 0.4, a = 1},
		},
	}
	wgpu.QueueWriteBuffer(
		queue,
		auto_cast globalsBuffer,
		0,
		&globals,
		size_of(globals),
	)

	for batch in batches {
		instanceCount := len(batch.modelMatrices)
		if (instanceCount == 0) {continue}
		wgpu.RenderPassEncoderSetBindGroup(passEncoder, 1, batch.bindGroup)
		wgpu.QueueWriteBuffer(
			queue,
			auto_cast batch.modelMatricesBuffer,
			0,
			raw_data(batch.modelMatrices),
			auto_cast instanceCount * size_of(batch.modelMatrices[0]),
		)

		wgpu.RenderPassEncoderDrawIndexed(
			passEncoder,
			indexCount,
			auto_cast instanceCount,
			0,
			0,
			0,
		)
	}
}

debugRendererEndFrame :: proc(using renderer: ^DebugRenderer) {
	frameBatchCount = 0
}

debugRendererAddBatch :: proc(
	using renderer: ^DebugRenderer,
	device: wgpu.Device,
	matrices: []common.mat4x4,
) {
	batch: ^DebugRendererInstanceBatch
	if (frameBatchCount < auto_cast len(batches)) {
		batch = &batches[frameBatchCount]
	} else {
		newBatch := debugRendererInstanceBatchCreate(
			device,
			pipeline.instanceUniformsGroupLayout,
		)
		append_elem(&batches, newBatch)

		batch = &batches[len(batches) - 1]
	}
	frameBatchCount += 1
	batch.modelMatrices = matrices
}

debugRendererSetAllBatches :: proc(
	using renderer: ^DebugRenderer,
	device: wgpu.Device,
	matrices: []common.mat4x4,
) {
	batchSize := DEBUGPIPELINE_INSTANCE_COUNT
	instanceCount := len(matrices)
	for i := 0; i < instanceCount; i += batchSize {
		start := i
		end := min(i + batchSize, instanceCount)
		count := end - start
		debugRendererAddBatch(renderer, device, matrices[start:end])
	}
}

debugRendererRelease :: proc(using renderer: ^DebugRenderer) {
	wgpu.RenderPipelineRelease(pipeline.pipeline)
	wgpu.BindGroupLayoutRelease(pipeline.globalUniformsGroupLayout)
	wgpu.BindGroupLayoutRelease(pipeline.instanceUniformsGroupLayout)

	wgpu.BufferRelease(auto_cast vertexBuffer)
	wgpu.BufferRelease(auto_cast indexBuffer)
	wgpu.BufferRelease(auto_cast globalsBuffer)
	wgpu.BindGroupRelease(globalsBindGroup)

	for batch in batches {
		wgpu.BufferRelease(auto_cast batch.modelMatricesBuffer)
		wgpu.BindGroupRelease(batch.bindGroup)
		delete(batch.modelMatrices)
	}
	delete(batches)
}
