package wgpuimpl

import "core:fmt"
import "core:mem"
import "vendor:wgpu"

WgpuVertexBuffer :: distinct wgpu.Buffer
WgpuIndexBuffer :: distinct wgpu.Buffer
WgpuUniformBuffer :: distinct wgpu.Buffer

bufferCreateVertex :: proc(
	device: wgpu.Device,
	data: []$T,
) -> WgpuVertexBuffer {
	byteLen := len(data) * size_of(T)
	buffer := wgpu.DeviceCreateBuffer(
		device,
		&wgpu.BufferDescriptor {
			usage = {wgpu.BufferUsageFlags.Vertex},
			size = auto_cast byteLen,
			mappedAtCreation = true,
		},
	)

	bufferData := wgpu.BufferGetMappedRangeSlice(
		buffer,
		0,
		T,
		len(data)
	)
	defer wgpu.BufferUnmap(buffer)

	mem.copy_non_overlapping(raw_data(bufferData), raw_data(data), byteLen)

	return cast(WgpuVertexBuffer)buffer
}

@(private = "file")
_bufferCreateIndex :: proc(
	device: wgpu.Device,
	data: []$T,
) -> WgpuIndexBuffer {
	byteLen := len(data) * size_of(T)
	buffer := wgpu.DeviceCreateBuffer(
		device,
		&wgpu.BufferDescriptor {
			usage = {wgpu.BufferUsageFlags.Index},
			size = auto_cast byteLen,
			mappedAtCreation = true,
		},
	)

	bufferData := wgpu.BufferGetMappedRangeSlice(
		buffer,
		0,
		T,
		len(data)
	)
	defer wgpu.BufferUnmap(buffer)

	mem.copy_non_overlapping(raw_data(bufferData), raw_data(data), byteLen)

	return auto_cast buffer
}

bufferCreateIndex_u16 :: proc(
	device: wgpu.Device,
	data: []u16,
) -> WgpuIndexBuffer {
	return _bufferCreateIndex(device, data)
}

bufferCreateIndex_u32 :: proc(
	device: wgpu.Device,
	data: []u32,
) -> WgpuIndexBuffer {
	return _bufferCreateIndex(device, data)
}

bufferCreateIndex :: proc {
	bufferCreateIndex_u16,
	bufferCreateIndex_u32,
}

bufferCreateUniform :: proc(
	device: wgpu.Device,
	$T: typeid,
	len: u32,
) -> WgpuUniformBuffer {
	byteLen := len * size_of(T)

	buffer := wgpu.DeviceCreateBuffer(
		device,
		&wgpu.BufferDescriptor {
			usage = {
				wgpu.BufferUsageFlags.Uniform,
				wgpu.BufferUsageFlags.CopyDst,
			},
			size = auto_cast byteLen,
			mappedAtCreation = false,
		},
	)

	return auto_cast buffer
}
