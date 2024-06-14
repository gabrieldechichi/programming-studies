import { mat4, vec2 } from "gl-matrix";
import {
  GPUUniformBuffer,
  GPUVertexBuffer,
  createUniformBuffer,
  createVertexBuffer,
} from "./bufferUtils";
import shaderSource from "./shader/debug.wgsl?raw";
import { Transform } from "./spriteRenderer";

export class DebugPipeline {
  pipeline!: GPURenderPipeline;
  uniformsBindGroup!: GPUBindGroup;
  uniformsBuffer!: GPUUniformBuffer;

  static create(device: GPUDevice, projectionViewBufer: GPUUniformBuffer) {
    const pipeline = new DebugPipeline();

    pipeline.uniformsBuffer = createUniformBuffer(
      device,
      Float32Array.BYTES_PER_ELEMENT * 4,
    );

    const uniformsGroupLayout = device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.VERTEX,
          buffer: { type: "uniform" },
        },
        {
          binding: 1,
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
        {
          binding: 1,
          resource: { buffer: pipeline.uniformsBuffer },
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
            //position
            {
              shaderLocation: 0,
              offset: 0,
              format: "float32x2",
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

export class DebugRenderer {
  device!: GPUDevice;
  projectionViewBuffer!: GPUUniformBuffer;
  pipeline!: DebugPipeline;
  squareVertexData!: Float32Array;
  squareVertexBuffer!: GPUVertexBuffer;

  private hackyRenderQueue: ((passEncoder: GPURenderPassEncoder) => void)[] =
    [];

  static create(device: GPUDevice, projectionViewBufer: GPUUniformBuffer) {
    const renderer = new DebugRenderer();
    renderer.device = device;
    renderer.projectionViewBuffer = projectionViewBufer;
    renderer.pipeline = DebugPipeline.create(device, projectionViewBufer);
    renderer.squareVertexData = new Float32Array(6 * 2);
    renderer.squareVertexBuffer = createVertexBuffer(
      device,
      renderer.squareVertexData,
    );

    return renderer;
  }

  startFrame(projectionViewMatrix: mat4) {
    this.device.queue.writeBuffer(
      this.projectionViewBuffer,
      0,
      projectionViewMatrix as Float32Array,
    );

    this.hackyRenderQueue = [];
  }

  drawSquare(transform: Transform) {
    this.hackyRenderQueue.push((passEncoder) => {
      const pos = transform.pos;
      const rot = transform.rot;
      const e = [transform.size[0] * 0.5, transform.size[1] * 0.5];

      const vs: vec2[] = [
        this.rotateVertex([pos[0] - e[0], pos[1] - e[1]], pos, rot),
        this.rotateVertex([pos[0] + e[0], pos[1] - e[1]], pos, rot),
        this.rotateVertex([pos[0] + e[0], pos[1] + e[1]], pos, rot),
        this.rotateVertex([pos[0] - e[0], pos[1] + e[1]], pos, rot),
      ];

      this.squareVertexData[0] = vs[0][0];
      this.squareVertexData[1] = vs[0][1];
      this.squareVertexData[2] = vs[1][0];
      this.squareVertexData[3] = vs[1][1];
      this.squareVertexData[4] = vs[2][0];
      this.squareVertexData[5] = vs[2][1];

      this.squareVertexData[6] = vs[2][0];
      this.squareVertexData[7] = vs[2][1];
      this.squareVertexData[8] = vs[3][0];
      this.squareVertexData[9] = vs[3][1];
      this.squareVertexData[10] = vs[0][0];
      this.squareVertexData[11] = vs[0][1];

      this.device.queue.writeBuffer(
        this.squareVertexBuffer,
        0,
        this.squareVertexData,
      );

      passEncoder.setPipeline(this.pipeline.pipeline);
      passEncoder.setBindGroup(0, this.pipeline.uniformsBindGroup);
      passEncoder.setVertexBuffer(0, this.squareVertexBuffer);
      passEncoder.draw(6);
    });
  }

  drawWireSquare(transform: Transform, thickness = 2){
      const {pos, size} = transform
      //left line
      this.drawSquare({pos: [pos[0] - size[0]/2 + thickness / 2, pos[1]], rot: 0, size: [thickness, size[1]]})
      //right line
      this.drawSquare({pos: [pos[0] + size[0]/2 - thickness / 2, pos[1]], rot: 0, size: [thickness, size[1]]})
      //bottom line
      this.drawSquare({pos: [pos[0], pos[1] - size[1]/2 + thickness/2], rot: 0, size: [size[0], thickness]})
      //top line
      this.drawSquare({pos: [pos[0], pos[1] + size[1]/2 - thickness/2], rot: 0, size: [size[0], thickness]})
  }

  endFrame(passEncoder: GPURenderPassEncoder) {
    for (const rend of this.hackyRenderQueue) {
      rend(passEncoder);
    }
  }

  rotateVertex(v: vec2, origin: vec2, rot: number): vec2 {
    return vec2.rotate(vec2.create(), v, origin, rot);
  }
}
