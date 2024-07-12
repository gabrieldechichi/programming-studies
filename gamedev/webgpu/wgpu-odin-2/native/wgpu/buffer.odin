package wgpuimpl

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
			size = byteLen,
			mappedAtCreation = true,
		},
	)

	bufferData := wgpu.BufferGetMappedRangeSlice(buffer, 0, T, byteLen)
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
			size = cast(u64)byteLen,
			mappedAtCreation = true,
		},
	)

	bufferData := wgpu.BufferGetMappedRangeSlice(
		buffer,
		0,
		T,
		cast(uint)byteLen,
	)
	defer wgpu.BufferUnmap(buffer)

	mem.copy_non_overlapping(raw_data(bufferData), raw_data(data), byteLen)

	return cast(WgpuIndexBuffer)buffer
}

bufferCreateIndex_i16 :: proc(
	device: wgpu.Device,
	data: []i16,
) -> WgpuIndexBuffer {
	return _bufferCreateIndex(device, data)
}

bufferCreateIndex_i32 :: proc(
	device: wgpu.Device,
	data: []i32,
) -> WgpuIndexBuffer {
	return _bufferCreateIndex(device, data)
}

bufferCreateIndex :: proc {
	bufferCreateIndex_i16,
	bufferCreateIndex_i32,
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
			size = cast(u64)byteLen,
			mappedAtCreation = false,
		},
	)

	return cast(WgpuUniformBuffer)buffer
}
