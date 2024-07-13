package vendor_wgpu_example_triangle

import "base:runtime"

import "core:fmt"
import "core:math/linalg"
import "core:math/rand"
import "lib:common"
import wgpuimpl "lib:wgpu"

import "vendor:wgpu"

State :: struct {
	ctx:                  runtime.Context,
	os:                   OS,
	instance:             wgpu.Instance,
	surface:              wgpu.Surface,
	adapter:              wgpu.Adapter,
	device:               wgpu.Device,
	renderFormat:         wgpu.TextureFormat,
	config:               wgpu.SurfaceConfiguration,
	queue:                wgpu.Queue,
	debugRenderer:        wgpuimpl.DebugRenderer,
	viewProjectionMatrix: common.mat4x4,
}

@(private = "file")
state: State

width: f32 = 600.0
height: f32 = 900.0
// MAX_INSTANCES :: BATCH_SIZE * 500
// MAX_INSTANCES :: BATCH_SIZE * 2
MAX_INSTANCES :: 10
BATCH_SIZE :: 1024
MAX_SPEED :: 200
BALL_RADIUS :: 5

balls: #soa[]Ball

Ball :: struct {
	position:  common.vec2,
	radius:    f32,
	velocity:  common.vec2,
	transform: common.mat4x4,
}

main :: proc() {
	simInit()
	renderInit()
}

simInit :: proc() {
	state.viewProjectionMatrix = linalg.matrix_ortho3d(
		-width / 2,
		width / 2,
		-height / 2,
		height / 2,
		-1,
		1,
	)

	balls = make_soa(#soa[]Ball, MAX_INSTANCES)

	rand.reset(1234)
	for i in 0 ..< len(balls) {

		x := rand.float32_range(-width / 2, width / 2)
		y := rand.float32_range(-height / 2, height / 2)
		vx := rand.float32_range(-1, 1) * MAX_SPEED
		vy := rand.float32_range(-1, 1) * MAX_SPEED

		balls[i] = {
			position = common.vec2{x, y},
			radius   = BALL_RADIUS,
			velocity = common.vec2{vx, vy},
		}
	}
}

resize :: proc "c" () {
	context = state.ctx

	state.config.width, state.config.height = os_get_render_bounds(&state.os)
	wgpu.SurfaceConfigure(state.surface, &state.config)
}

frame :: proc "c" (dt: f32) {
	context = state.ctx

	simulate(dt)

	renderFrame()
}

simulate :: proc(dt: f32) {

	using state

	if (len(balls) == 0) {return}

	for &ball in balls {
		ball.position += ball.velocity * dt
		if (ball.position.x + ball.radius > width / 2 && ball.velocity.x > 0) {
			ball.velocity.x *= -1
		} else if (ball.position.x - ball.radius < -width / 2 &&
			   ball.velocity.x < 0) {
			ball.velocity.x *= -1
		}
		if (ball.position.y + ball.radius > height / 2 &&
			   ball.velocity.y > 0) {
			ball.velocity.y *= -1
		} else if (ball.position.y - ball.radius < -height / 2 &&
			   ball.velocity.y < 0) {
			ball.velocity.y *= -1
		}

		ball.transform = linalg.matrix4_from_trs(
			common.vec3{ball.position.x, ball.position.y, 0},
			linalg.QUATERNIONF32_IDENTITY,
			common.vec3{ball.radius * 2, ball.radius * 2, 0},
		)
	}

	wgpuimpl.debugRendererSetAllBatches(
		&debugRenderer,
		device,
		balls.transform[0:len(balls)],
	)
}

finish :: proc() {
	wgpuimpl.debugRendererRelease(&state.debugRenderer)
	wgpu.QueueRelease(state.queue)
	wgpu.DeviceRelease(state.device)
	wgpu.AdapterRelease(state.adapter)
	wgpu.SurfaceRelease(state.surface)
	wgpu.InstanceRelease(state.instance)
}

renderInit :: proc() {
	state.ctx = context

	os_init(&state.os)

	state.instance = wgpu.CreateInstance(nil)
	assert(state.instance != nil)
	state.surface = os_get_surface(&state.os, state.instance)

	wgpu.InstanceRequestAdapter(
		state.instance,
		&{
			compatibleSurface = state.surface,
			powerPreference = wgpu.PowerPreference.HighPerformance,
		},
		on_adapter,
		nil,
	)

	on_adapter :: proc "c" (
		status: wgpu.RequestAdapterStatus,
		adapter: wgpu.Adapter,
		message: cstring,
		userdata: rawptr,
	) {
		context = state.ctx
		if status != .Success || adapter == nil {
			fmt.panicf("request adapter failure: [%v] %s", status, message)
		}
		state.adapter = adapter
		wgpu.AdapterRequestDevice(adapter, nil, on_device)
	}

	on_device :: proc "c" (
		status: wgpu.RequestDeviceStatus,
		device: wgpu.Device,
		message: cstring,
		userdata: rawptr,
	) {
		context = state.ctx
		if status != .Success || device == nil {
			fmt.panicf("request device failure: [%v] %s", status, message)
		}
		state.device = device

		width, height := os_get_render_bounds(&state.os)

		state.renderFormat = wgpu.SurfaceGetPreferredFormat(
			state.surface,
			state.adapter,
		)
		state.config = wgpu.SurfaceConfiguration {
			device      = state.device,
			usage       = {.RenderAttachment},
			format      = state.renderFormat,
			width       = width,
			height      = height,
			presentMode = .Fifo,
			alphaMode   = .Opaque,
		}
		wgpu.SurfaceConfigure(state.surface, &state.config)

		state.queue = wgpu.DeviceGetQueue(state.device)

		state.debugRenderer = wgpuimpl.debugRendererCreate(
			{device = state.device, textureFormat = state.renderFormat},
		)

		os_run(&state.os)
	}
}

renderFrame :: proc() {
	surface_texture := wgpu.SurfaceGetCurrentTexture(state.surface)
	switch surface_texture.status {
	case .Success:
	// All good, could check for `surface_texture.suboptimal` here.
	case .Timeout, .Outdated, .Lost:
		// Skip this frame, and re-configure surface.
		if surface_texture.texture != nil {
			wgpu.TextureRelease(surface_texture.texture)
		}
		resize()
		return
	case .OutOfMemory, .DeviceLost:
		// Fatal error
		fmt.panicf(
			"[triangle] get_current_texture status=%v",
			surface_texture.status,
		)
	}
	defer wgpu.TextureRelease(surface_texture.texture)

	frame := wgpu.TextureCreateView(surface_texture.texture, nil)
	defer wgpu.TextureViewRelease(frame)

	command_encoder := wgpu.DeviceCreateCommandEncoder(state.device, nil)
	defer wgpu.CommandEncoderRelease(command_encoder)

	render_pass_encoder := wgpu.CommandEncoderBeginRenderPass(
		command_encoder,
		&{
			colorAttachmentCount = 1,
			colorAttachments = &wgpu.RenderPassColorAttachment {
				view = frame,
				loadOp = .Clear,
				storeOp = .Store,
				clearValue = {r = 0.2, g = 0.2, b = 0.2, a = 1},
			},
		},
	)
	defer wgpu.RenderPassEncoderRelease(render_pass_encoder)

	wgpuimpl.debugRendererRender(
		state.debugRenderer,
		render_pass_encoder,
		state.queue,
		state.viewProjectionMatrix,
	)
	defer wgpuimpl.debugRendererEndFrame(&state.debugRenderer)

	wgpu.RenderPassEncoderEnd(render_pass_encoder)

	command_buffer := wgpu.CommandEncoderFinish(command_encoder, nil)
	defer wgpu.CommandBufferRelease(command_buffer)

	wgpu.QueueSubmit(state.queue, {command_buffer})
	wgpu.SurfacePresent(state.surface)
}
