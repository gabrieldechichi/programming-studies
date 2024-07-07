import {
  GPUIndexBuffer,
  GPUUniformBuffer,
  GPUVertexBuffer,
  WGPUBuffer,
} from "src/wgpu/buffer";
import {
  DebugPipeline,
  DebugPipelineCreateParams,
  DebugPipelineGlobalUniforms,
  DebugPipelineModelMatrixUniforms,
} from "src/wgpu/pipelines/debugPipeline";

export type DebugRenderInstance = {
  modelMatrix: Float32Array;
};

export class DebugRenderer {
  pipeline!: DebugPipeline;
  vertexBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;
  indexCount!: number;
  globalsBuffer!: GPUUniformBuffer;
  modelMatricesBuffer!: GPUUniformBuffer;
  modelMatricesData!: Float32Array;
  uniformsBindGroup!: GPUBindGroup;

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

    renderer.globalsBuffer = WGPUBuffer.createUniformBuffer(
      device,
      DebugPipelineGlobalUniforms.byteSize,
    );
    renderer.modelMatricesBuffer = WGPUBuffer.createUniformBuffer(
      device,
      DebugPipelineModelMatrixUniforms.byteSize,
    );

    renderer.uniformsBindGroup = device.createBindGroup({
      layout: renderer.pipeline.uniformsGroupLayout,
      entries: [
        //globals
        {
          binding: 0,
          resource: { buffer: renderer.globalsBuffer },
        },
        //matrices
        {
          binding: 1,
          resource: { buffer: renderer.modelMatricesBuffer },
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

    renderer.modelMatricesData = new Float32Array(
      DebugPipelineModelMatrixUniforms.modelMatricesFloatCount,
    );

    return renderer;
  }

  addInstance(instance: DebugRenderInstance) {
    if (this.instanceCount < DebugPipelineModelMatrixUniforms.instanceCount) {
      const offset = this.instanceCount * DebugPipelineModelMatrixUniforms.elementByteSize;
      this.modelMatricesData.set(instance.modelMatrix, offset);
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
    if (this.instanceCount <= 0) {
      return;
    }
    passEncoder.setPipeline(this.pipeline.pipeline);
    passEncoder.setVertexBuffer(0, this.vertexBuffer);
    passEncoder.setIndexBuffer(this.indexBuffer, "uint16");
    passEncoder.setBindGroup(0, this.uniformsBindGroup);

    queue.writeBuffer(
      this.globalsBuffer,
      DebugPipelineGlobalUniforms.viewProjectionMatrixOffset,
      projectionViewMatrix,
    );

    queue.writeBuffer(
      this.modelMatricesBuffer,
      DebugPipelineModelMatrixUniforms.modelMatricesOffset,
      this.modelMatricesData,
      0,
      this.instanceCount * DebugPipelineModelMatrixUniforms.elementByteSize,
    );

    passEncoder.drawIndexed(this.indexCount, this.instanceCount);
  }

  endFrame() {
    this.instanceCount = 0;
  }
}
