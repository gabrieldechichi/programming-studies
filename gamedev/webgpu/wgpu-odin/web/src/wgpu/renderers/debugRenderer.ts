import {
  GPUIndexBuffer,
  GPUUniformBuffer,
  GPUVertexBuffer,
  WGPUBuffer,
} from "src/wgpu/buffer";
import {
  DebugPipeline,
  DebugPipelineCreateParams,
  DebugPipelineUniforms,
} from "src/wgpu/pipelines/debugPipeline";

export type DebugRenderInstance = {
  modelMatrix: Float32Array;
};

export class DebugRenderer {
  pipeline!: DebugPipeline;
  vertexBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;
  indexCount!: number;
  uniformMatricesData!: Float32Array;
  uniformsBuffer!: GPUUniformBuffer;
  uniformsBindGroup!: GPUBindGroup;

  //todo: fixed array
  instanceCount: number = 0;

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

    renderer.uniformMatricesData = new Float32Array(
      DebugPipelineUniforms.modelMatricesFloatCount,
    );

    return renderer;
  }

  addInstance(instance: DebugRenderInstance) {
    if (this.instanceCount < DebugPipelineUniforms.instanceCount) {
      const offset = this.instanceCount * 16;
      this.uniformMatricesData.set(instance.modelMatrix, offset);
      this.instanceCount++;
    }
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

    queue.writeBuffer(
      this.uniformsBuffer,
      DebugPipelineUniforms.viewProjectionMatrixOffset,
      projectionViewMatrix,
    );

    queue.writeBuffer(
      this.uniformsBuffer,
      DebugPipelineUniforms.modelMatricesOffset,
      this.uniformMatricesData,
      0,
      this.instanceCount * 16,
    );

    passEncoder.drawIndexed(this.indexCount, this.instanceCount);
  }

  endFrame() {
    this.instanceCount = 0;
  }
}
