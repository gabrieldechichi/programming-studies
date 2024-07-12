package wgpuimpl

import "lib:common"
import "vendor:wgpu"

DebugRenderer :: struct {
	pipeline:         DebugPipeline,
	vertexBuffer:     WgpuVertexBuffer,
	indexBuffer:      WgpuIndexBuffer,
	indexCount:       u32,
	globalsBuffer:    WgpuUniformBuffer,
	globalsBindGroup: wgpu.BindGroup,
}

DebugRendererInstanceBatch :: struct {
	modelMatricesBuffer: WgpuUniformBuffer,
	modelMatrices:       []common.mat4x4,
	bindGroup:           wgpu.BindGroup,
	instanceCount:       u32,
}

debugRendererInstanceBatchCreate :: proc(
	device: wgpu.Device,
	groupLayout: wgpu.BindGroupLayout,
) -> DebugRendererInstanceBatch {

	batch := DebugRendererInstanceBatch{}
	batch.modelMatricesBuffer = bufferCreateUniform(
		device,
		DebugPipelineModelMatrixUniforms,
		DEBUGPIPELINE_INSTANCE_COUNT,
	)
	batch.modelMatrices = make([]common.mat4x4, DEBUGPIPELINE_INSTANCE_COUNT)
	batch.bindGroup = wgpu.DeviceCreateBindGroup(
		device,
		&wgpu.BindGroupDescriptor {
			layout = &groupLayout,
			entryCount = 0,
			entries = raw_data(
				[]wgpu.BindGroupEntry {
					{binding = 0, buffer = batch.modelMatricesBuffer},
				},
			),
		},
	)
	return batch
}
