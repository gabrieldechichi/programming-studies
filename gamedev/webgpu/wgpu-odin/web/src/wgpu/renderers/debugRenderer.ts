import {
  GPUIndexBuffer,
  GPUUniformBuffer,
  GPUVertexBuffer,
  WGPUBuffer,
} from "../buffer";
import {
  DebugPipeline,
  DebugPipelineCreateParams,
  DebugPipelineUniforms,
} from "../pipelines/debugPipeline";

export type DebugRenderInstance = {
  modelMatrix: Float32Array;
};

export class DebugRenderer {
  pipeline!: DebugPipeline;
  vertexBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;
  indexCount!: number;
  uniformsBuffer!: GPUUniformBuffer;
  uniformsBindGroup!: GPUBindGroup;

  //todo: fixed array
  instances: DebugRenderInstance[] = [];

  //prettier-ignore
  static SpriteUIGeo = new Float32Array([
    //pos       //uv
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    0.0, 1.0,
  ]);

  //prettier-ignore
  static SpriteCenteredGeo = new Float32Array([
    //pos          //uv
    -0.5, -0.5,
    0.5, -0.5,
    0.5, 0.5,
    -0.5, 0.5,
  ]);

  static create(pipelineCreateData: DebugPipelineCreateParams) {
    const { device } = pipelineCreateData;
    const renderer = new DebugRenderer();
    renderer.pipeline = DebugPipeline.create(pipelineCreateData);

    renderer.uniformsBuffer = WGPUBuffer.createUniformBuffer(
      device,
      DebugPipelineUniforms.byteSize,
    );
    renderer.uniformsBindGroup = device.createBindGroup({
      layout: renderer.pipeline.uniformsGroupLayout,
      entries: [
        {
          binding: 0,
          resource: { buffer: renderer.uniformsBuffer },
        },
      ],
    });

    renderer.vertexBuffer = WGPUBuffer.createVertexBuffer(
      device,
      this.SpriteCenteredGeo,
    );

    const indexArray = new Int16Array([0, 1, 2, 0, 2, 3]);
    renderer.indexBuffer = WGPUBuffer.createIndexBuffer(device, indexArray);
    renderer.indexCount = indexArray.length;

    return renderer;
  }

  addInstance(instance: DebugRenderInstance) {
    this.instances.push(instance);
  }

  render({
    passEncoder,
    queue,
    projectionViewMatrix,
  }: {
    passEncoder: GPURenderPassEncoder;
    projectionViewMatrix: Float32Array;
    queue: GPUQueue;
  }) {
    passEncoder.setPipeline(this.pipeline.pipeline);
    passEncoder.setVertexBuffer(0, this.vertexBuffer);
    passEncoder.setIndexBuffer(this.indexBuffer, "uint16");
    passEncoder.setBindGroup(0, this.uniformsBindGroup);
    for (let i = 0; i < this.instances.length; i++) {
      const instance = this.instances[i];

      queue.writeBuffer(
        this.uniformsBuffer,
        DebugPipelineUniforms.modelMatrixOffset,
        instance.modelMatrix,
      );

      queue.writeBuffer(
        this.uniformsBuffer,
        DebugPipelineUniforms.viewProjectionMatrixOffset,
        projectionViewMatrix,
      );

      passEncoder.drawIndexed(this.indexCount, 1);
    }
  }

  endFrame() {
    this.instances.length = 0;
  }
}
