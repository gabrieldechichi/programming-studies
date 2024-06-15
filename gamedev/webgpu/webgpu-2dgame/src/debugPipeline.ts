import { vec2 } from "gl-matrix";
import { GPUUniformBuffer } from "./rendering/bufferUtils";
import shaderSource from "./shader/debug.wgsl?raw";
import { InstanceData } from "./rendering/instancing";
import { MathUtils, Transform } from "./math/math";

export class DebugPipeline {
  pipeline!: GPURenderPipeline;
  uniformsBindGroup!: GPUBindGroup;

  static VERTEX_INSTANCE_FLOAT_NUM = 16 + 4; //mat4x4 + color

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
          arrayStride:
            DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM *
            Float32Array.BYTES_PER_ELEMENT,
          stepMode: "instance",
          attributes: [
            //model matrix line 1
            {
              shaderLocation: 1,
              offset: 0,
              format: "float32x4",
            },
            //model matrix line 2
            {
              shaderLocation: 2,
              offset: 4 * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
            //model matrix line 3
            {
              shaderLocation: 3,
              offset: (4 + 4) * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
            //model matrix line 4
            {
              shaderLocation: 4,
              offset: (4 + 4 + 4) * Float32Array.BYTES_PER_ELEMENT,
              format: "float32x4",
            },
            //color
            {
              shaderLocation: 5,
              offset: 16 * Float32Array.BYTES_PER_ELEMENT,
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

const MAX_INSTANCES = 100;

export class DebugRenderer {
  device!: GPUDevice;
  pipeline!: DebugPipeline;
  squareInstanceData!: InstanceData;
  circleInstanceData!: InstanceData;

  static create(device: GPUDevice, projectionViewBufer: GPUUniformBuffer) {
    const renderer = new DebugRenderer();
    renderer.device = device;
    renderer.pipeline = DebugPipeline.create(device, projectionViewBufer);

    //squares buffer
    {
      renderer.squareInstanceData = InstanceData.create(
        device,
        MAX_INSTANCES,
        DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
        //prettier-ignore
        new Float32Array([
          //tri 1
        -0.5, -0.5,
        0.5, -0.5,
        0.5, 0.5,
        -0.5, 0.5,
      ]),
        new Int16Array([0, 1, 2, 2, 3, 0]),
      );
    }

    //circle buffers
    {
      const sectorCount = 24;
      const radius = 1;
      const circleGeometryData: number[] = [0, 0];
      const circleIndexData: number[] = [];
      // Calculate surrounding vertices
      for (let i = 0; i <= sectorCount; ++i) {
        const angle = (i / sectorCount) * 2 * Math.PI;
        const x = Math.cos(angle) * radius;
        const y = Math.sin(angle) * radius;
        circleGeometryData.push(x, y);
      }

      for (let i = 1; i <= sectorCount; ++i) {
        circleIndexData.push(0, i, i + 1);
      }
      renderer.circleInstanceData = InstanceData.create(
        device,
        MAX_INSTANCES,
        DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
        new Float32Array(circleGeometryData),
        new Int16Array(circleIndexData),
      );
    }

    return renderer;
  }

  startFrame() {
    this.squareInstanceData.count = 0;
    this.circleInstanceData.count = 0;
  }

  endFrame(passEncoder: GPURenderPassEncoder) {
    this.drawInstancedData(passEncoder, this.squareInstanceData);
    this.drawInstancedData(passEncoder, this.circleInstanceData);
  }

  drawCircle(
    pos: vec2,
    radius: number,
    color: GPUColorDict = { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
  ) {
    DebugRenderer.fillInstanceData(
      this.circleInstanceData,
      {
        pos,
        size: [radius, radius],
        rot: 0,
      },
      color,
    );
  }

  drawWireCircle(
    pos: vec2,
    radius: number,
    thickness: number = 2,
    color: GPUColorDict = { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
  ) {
    const sectorCount = 24;
    for (let i = 0; i <= sectorCount - 1; ++i) {
      const angleA = (i / sectorCount) * 2 * Math.PI;
      const angleB = ((i + 1) / sectorCount) * 2 * Math.PI;
      const xA = Math.cos(angleA) * radius;
      const yA = Math.sin(angleA) * radius;
      const xB = Math.cos(angleB) * radius;
      const yB = Math.sin(angleB) * radius;
      const length = Math.sqrt((xB - xA) * (xB - xA) + (yB - yA) * (yB - yA));

      const x = (xB + xA) / 2;
      const y = (yB + yA) / 2;
      const rot = (angleB + angleA) / 2 - Math.PI / 2;
      this.drawSquare(
        { pos: [x + pos[0], y + pos[1]], rot, size: [length, thickness] },
        color,
      );
    }
  }

  drawSquare(
    transform: Transform,
    color: GPUColorDict = { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
  ) {
    DebugRenderer.fillInstanceData(this.squareInstanceData, transform, color);
  }

  drawWireSquare(
    transform: Transform,
    thickness = 2,
    color: GPUColorDict = { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
  ) {
    const { pos, rot, size } = transform;

    //left line
    this.drawSquare(
      {
        pos: MathUtils.rotateVertex(
          [pos[0] - size[0] / 2 + thickness / 2, pos[1]],
          pos,
          rot,
        ),
        rot,
        size: [thickness, size[1]],
      },
      color,
    );
    //right line
    this.drawSquare(
      {
        pos: MathUtils.rotateVertex(
          [pos[0] + size[0] / 2 - thickness / 2, pos[1]],
          pos,
          rot,
        ),
        rot,
        size: [thickness, size[1]],
      },
      color,
    );
    //bottom line
    this.drawSquare(
      {
        pos: MathUtils.rotateVertex(
          [pos[0], pos[1] - size[1] / 2 + thickness / 2],
          pos,
          rot,
        ),
        rot,
        size: [size[0], thickness],
      },
      color,
    );
    //top line
    this.drawSquare(
      {
        pos: MathUtils.rotateVertex(
          [pos[0], pos[1] + size[1] / 2 - thickness / 2],
          pos,
          rot,
        ),
        rot,
        size: [size[0], thickness],
      },
      color,
    );
  }

  private drawInstancedData(
    passEncoder: GPURenderPassEncoder,
    instanceData: InstanceData,
  ) {
    if (instanceData.count > 0) {
      this.device.queue.writeBuffer(
        instanceData.instancesBuffer,
        0,
        instanceData.data,
      );

      passEncoder.setPipeline(this.pipeline.pipeline);
      passEncoder.setBindGroup(0, this.pipeline.uniformsBindGroup);
      passEncoder.setIndexBuffer(instanceData.indexBuffer, "uint16");
      passEncoder.setVertexBuffer(0, instanceData.geometryBuffer);
      passEncoder.setVertexBuffer(1, instanceData.instancesBuffer);

      passEncoder.drawIndexed(instanceData.indexCount, instanceData.count);
    }
  }

  private static fillInstanceData(
    instanceData: InstanceData,
    transform: Transform,
    color: GPUColorDict,
  ) {
    //TODO: handle requests above MAX_INSTANCES
    const index = instanceData.count * instanceData.floatStride;

    const modelMatrix = MathUtils.trs(transform);

    for (let i = 0; i < 16; i++) {
      instanceData.data[index + i] = modelMatrix[i];
    }

    const colorOffset = index + 16;
    //rgba
    instanceData.data[colorOffset + 0] = color.r;
    instanceData.data[colorOffset + 1] = color.g;
    instanceData.data[colorOffset + 2] = color.b;
    instanceData.data[colorOffset + 3] = color.a;

    instanceData.count++;
  }
}
