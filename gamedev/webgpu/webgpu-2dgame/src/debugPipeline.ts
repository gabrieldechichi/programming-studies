import { mat4, quat, vec2 } from "gl-matrix";
import {
  GPUIndexBuffer,
  GPUUniformBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";
import shaderSource from "./shader/debug.wgsl?raw";
import { Transform } from "./spriteRenderer";

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
          //instancePos + instance size + color
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

type InstanceData = {
  floatStride: number;
  data: Float32Array;
  geometryBuffer: GPUVertexBuffer;
  indexBuffer: GPUIndexBuffer;
  indexCount: number;
  instancesBuffer: GPUVertexBuffer;
  capacity: number;
  count: number;
};

export class DebugRenderer {
  device!: GPUDevice;
  projectionViewBuffer!: GPUUniformBuffer;
  pipeline!: DebugPipeline;
  squareInstanceData!: InstanceData;
  circleInstanceData!: InstanceData;
  //vertex data for a single square

  static create(device: GPUDevice, projectionViewBufer: GPUUniformBuffer) {
    const renderer = new DebugRenderer();
    renderer.device = device;
    renderer.projectionViewBuffer = projectionViewBufer;
    renderer.pipeline = DebugPipeline.create(device, projectionViewBufer);

    //@ts-ignore
    renderer.squareInstanceData = {
      floatStride: DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
      data: new Float32Array(
        MAX_INSTANCES * DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
      ),
      capacity: MAX_INSTANCES,
      count: 0,
    };
    renderer.squareInstanceData.instancesBuffer = createVertexBuffer(
      device,
      renderer.squareInstanceData.data,
    );

    //prettier-ignore
    renderer.squareInstanceData.geometryBuffer = createVertexBuffer(
      device,
      new Float32Array([
          //tri 1
        -0.5, -0.5,
        0.5, -0.5,
        0.5, 0.5,
        -0.5, 0.5,
      ]),
    );

    renderer.squareInstanceData.indexBuffer = createIndexBuffer(
      device,
      new Int16Array([0, 1, 2, 2, 3, 0]),
    );

    renderer.squareInstanceData.indexCount = 6;

    //@ts-ignore
    renderer.circleInstanceData = {
      floatStride: DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
      data: new Float32Array(
        MAX_INSTANCES * DebugPipeline.VERTEX_INSTANCE_FLOAT_NUM,
      ),
      capacity: MAX_INSTANCES,
      count: 0,
    };
    renderer.circleInstanceData.instancesBuffer = createVertexBuffer(
      device,
      renderer.circleInstanceData.data,
    );

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
    renderer.circleInstanceData.indexCount = circleIndexData.length;

    renderer.circleInstanceData.geometryBuffer = createVertexBuffer(
      device,
      new Float32Array(circleGeometryData),
    );

    renderer.circleInstanceData.indexBuffer = createIndexBuffer(
      device,
      new Int16Array(circleIndexData),
    );

    return renderer;
  }

  startFrame(projectionViewMatrix: mat4) {
    this.device.queue.writeBuffer(
      this.projectionViewBuffer,
      0,
      projectionViewMatrix as Float32Array,
    );

    this.squareInstanceData.count = 0;
    this.circleInstanceData.count = 0;
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
        pos: this.rotateVertex(
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
        pos: this.rotateVertex(
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
        pos: this.rotateVertex(
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
        pos: this.rotateVertex(
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

  endFrame(passEncoder: GPURenderPassEncoder) {
    this.drawInstancedData(passEncoder, this.squareInstanceData);
    this.drawInstancedData(passEncoder, this.circleInstanceData);
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
    const index = instanceData.count * instanceData.floatStride;
    //TODO: random requests above MAX_INSTANCES

    const modelMatrix = DebugRenderer.trs(transform);

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

  private static trs(transform: Transform) {
    return mat4.fromRotationTranslationScale(
      mat4.create(),
      quat.fromEuler(quat.create(), 0, 0, (180 * transform.rot) / Math.PI),
      [transform.pos[0], transform.pos[1], 0],
      [transform.size[0], transform.size[1], 0],
    );
  }

  rotateVertex(v: vec2, origin: vec2, rot: number): vec2 {
    return vec2.rotate(vec2.create(), v, origin, rot);
  }
}
