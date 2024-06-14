import { mat4, vec2 } from "gl-matrix";
import {
  GPUUniformBuffer,
  GPUVertexBuffer,
  createVertexBuffer,
} from "./bufferUtils";
import shaderSource from "./shader/debug.wgsl?raw";
import { Transform } from "./spriteRenderer";

export class DebugPipeline {
  pipeline!: GPURenderPipeline;
  uniformsBindGroup!: GPUBindGroup;

  static VERTEX_INSTANCE_FLOAT_NUM = 2 + 2 + 4;

  static create(device: GPUDevice, projectionViewBufer: GPUUniformBuffer) {
    const pipeline = new DebugPipeline();

    const uniformsGroupLayout = device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
      ],
    });

    pipeline.uniformsBindGroup = device.createBindGroup({
      layout: uniformsGroupLayout,
      entries: [
        {
          binding: 0,
          resource: { buffer: projectionViewBufer },
        },
      ],
    });

    const module = device.createShaderModule({ code: shaderSource });

    const vertex: GPUVertexState = {
      module,
      entryPoint: "vertexMain",
      buffers: [
        {
          arrayStride: 2 * Float32Array.BYTES_PER_ELEMENT,
          stepMode: "vertex",
          attributes: [
            //vertex Pos
            {
              shaderLocation: 0,
              offset: 0,
              format: "float32x2",
            },
          ],
        },
        {
          //instancePos + instance size + color
          arrayStride:
            DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM *
            Float32Array.BYTES_PER_ELEMENT,
          stepMode: "instance",
          attributes: [
            //instance pos
            {
              shaderLocation: 1,
              offset: 0,
              format: "float32x2",
            },
            //instance size
            {
              shaderLocation: 2,
              offset: 2 * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x2",
            },
            //color
            {
              shaderLocation: 3,
              offset: (2 + 2) * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
          ],
        },
      ],
    };

    const fragment: GPUFragmentState = {
      module,
      entryPoint: "fragmentMain",
      targets: [
        {
          format: navigator.gpu.getPreferredCanvasFormat(),
          blend: {
            color: {
              operation: "add",
              srcFactor: "src-alpha",
              dstFactor: "one-minus-src-alpha",
            },
            alpha: {
              operation: "add",
              srcFactor: "one",
              dstFactor: "one-minus-src-alpha",
            },
          },
        },
      ],
    };

    pipeline.pipeline = device.createRenderPipeline({
      label: "debug",
      vertex,
      fragment,
      primitive: { topology: "triangle-list" },
      layout: device.createPipelineLayout({
        bindGroupLayouts: [uniformsGroupLayout],
      }),
    });
    return pipeline;
  }
}

const MAX_INSTANCES = 1024;

type InstanceData = {
  floatStride: number;
  data: Float32Array;
  buffer: GPUVertexBuffer;
  capacity: number;
  count: number;
};

export class DebugRenderer {
  device!: GPUDevice;
  projectionViewBuffer!: GPUUniformBuffer;
  pipeline!: DebugPipeline;
  instanceData!: InstanceData;
  //vertex data for a single square
  squareVertexBuffer!: GPUVertexBuffer;

  static create(device: GPUDevice, projectionViewBufer: GPUUniformBuffer) {
    const renderer = new DebugRenderer();
    renderer.device = device;
    renderer.projectionViewBuffer = projectionViewBufer;
    renderer.pipeline = DebugPipeline.create(device, projectionViewBufer);
    //prettier-ignore
    renderer.squareVertexBuffer = createVertexBuffer(
      device,
      new Float32Array([
          //tri 1
        -0.5, -0.5,
        0.5, -0.5,
        0.5, 0.5,

        //tri 2
        0.5, 0.5,
        -0.5, 0.5,
        -0.5, -0.5,
      ]),
    );

    //@ts-ignore
    renderer.instanceData = {
      floatStride: DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
      data: new Float32Array(
        MAX_INSTANCES * DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
      ),
      capacity: MAX_INSTANCES,
      count: 0,
    };
    renderer.instanceData.buffer = createVertexBuffer(
      device,
      renderer.instanceData.data,
    );

    return renderer;
  }

  startFrame(projectionViewMatrix: mat4) {
    this.device.queue.writeBuffer(
      this.projectionViewBuffer,
      0,
      projectionViewMatrix as Float32Array,
    );

    this.instanceData.count = 0;
  }

  drawSquare(
    transform: Transform,
    color: GPUColor = { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
  ) {
    const index = this.instanceData.count * this.instanceData.floatStride;
    //TODO: random requests above MAX_INSTANCES

    this.instanceData.data[index + 0] = transform.pos[0];
    this.instanceData.data[index + 1] = transform.pos[1];

    this.instanceData.data[index + 2] = transform.size[0];
    this.instanceData.data[index + 3] = transform.size[1];

    //rgba
    this.instanceData.data[index + 4] = color.r;
    this.instanceData.data[index + 5] = color.g;
    this.instanceData.data[index + 6] = color.b;
    this.instanceData.data[index + 7] = color.a;

    this.instanceData.count++;
  }

  drawWireSquare(
    transform: Transform,
    thickness = 2,
    color: GPUColor = { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
  ) {
    const { pos, size } = transform;
    //left line
    this.drawSquare({
      pos: [pos[0] - size[0] / 2 + thickness / 2, pos[1]],
      rot: 0,
      size: [thickness, size[1]],
    }, color);
    //right line
    this.drawSquare({
      pos: [pos[0] + size[0] / 2 - thickness / 2, pos[1]],
      rot: 0,
      size: [thickness, size[1]],
    }, color);
    //bottom line
    this.drawSquare({
      pos: [pos[0], pos[1] - size[1] / 2 + thickness / 2],
      rot: 0,
      size: [size[0], thickness],
    }, color);
    //top line
    this.drawSquare({
      pos: [pos[0], pos[1] + size[1] / 2 - thickness / 2],
      rot: 0,
      size: [size[0], thickness],
    }, color);
  }

  endFrame(passEncoder: GPURenderPassEncoder) {
    this.device.queue.writeBuffer(
      this.instanceData.buffer,
      0,
      this.instanceData.data,
    );

    passEncoder.setPipeline(this.pipeline.pipeline);
    passEncoder.setBindGroup(0, this.pipeline.uniformsBindGroup);
    passEncoder.setVertexBuffer(0, this.squareVertexBuffer);
    passEncoder.setVertexBuffer(1, this.instanceData.buffer);

    passEncoder.draw(6, this.instanceData.count);
  }

  rotateVertex(v: vec2, origin: vec2, rot: number): vec2 {
    return vec2.rotate(vec2.create(), v, origin, rot);
  }
}
