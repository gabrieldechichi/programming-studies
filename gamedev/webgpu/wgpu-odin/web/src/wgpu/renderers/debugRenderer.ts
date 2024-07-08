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

export class DebugRendererInstanceBatch {
  modelMatricesBuffer!: GPUUniformBuffer;
  modelMatricesData!: Float32Array;
  instanceDataBindGroup!: GPUBindGroup;

  instanceCount: number = 0;

  static create(
    device: GPUDevice,
    groupLayout: GPUBindGroupLayout,
  ): DebugRendererInstanceBatch {
    const batch = new DebugRendererInstanceBatch();
    batch.modelMatricesBuffer = WGPUBuffer.createUniformBuffer(
      device,
      DebugPipelineModelMatrixUniforms.byteSize,
    );
    batch.instanceDataBindGroup = device.createBindGroup({
      layout: groupLayout,
      entries: [
        //matrices
        {
          binding: 0,
          resource: { buffer: batch.modelMatricesBuffer },
        },
      ],
    });

    batch.modelMatricesData = new Float32Array(
      DebugPipelineModelMatrixUniforms.modelMatricesFloatCount,
    );
    return batch;
  }
}

export class DebugRenderer {
  pipeline!: DebugPipeline;
  vertexBuffer!: GPUVertexBuffer;
  indexBuffer!: GPUIndexBuffer;
  indexCount!: number;
  globalsBuffer!: GPUUniformBuffer;
  globalUniformsBindGroup!: GPUBindGroup;

  instanceBatches: DebugRendererInstanceBatch[] = [];
  instanceBatchesLen: number = 0;

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

    renderer.globalUniformsBindGroup = device.createBindGroup({
      layout: renderer.pipeline.globalUniformsGroupLayout,
      entries: [
        //globals
        {
          binding: 0,
          resource: { buffer: renderer.globalsBuffer },
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

  addInstanceBatch(device: GPUDevice, matrices: Float32Array, count: number) {
    let newBatch: DebugRendererInstanceBatch | null = null;
    if (this.instanceBatchesLen < this.instanceBatches.length) {
      newBatch = this.instanceBatches[this.instanceBatchesLen];
    } else {
      newBatch = DebugRendererInstanceBatch.create(
        device,
        this.pipeline.instanceUniformsGroupLayout,
      );
      this.instanceBatches.push(newBatch);
    }
    this.instanceBatchesLen++

    newBatch.instanceCount = 0;
    newBatch.modelMatricesData.set(matrices);
    newBatch.instanceCount = count;
  }

  setAllBatches(
    device: GPUDevice,
    matrices: Float32Array,
    instanceCount: number,
    instanceFloatCount: number,
  ) {
    const batchSize = DebugPipelineModelMatrixUniforms.instanceCount;
    for (let i = 0; i < instanceCount; i += batchSize) {
      const start = i;
      const end = Math.min(i + batchSize, instanceCount);
      const count = end - start;
      this.addInstanceBatch(
        device,
        matrices.subarray(start * instanceFloatCount, end * instanceFloatCount),
        count,
      );
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
    if (this.instanceBatchesLen <= 0) {
      return;
    }
    passEncoder.setPipeline(this.pipeline.pipeline);
    passEncoder.setVertexBuffer(0, this.vertexBuffer);
    passEncoder.setIndexBuffer(this.indexBuffer, "uint16");
    passEncoder.setBindGroup(0, this.globalUniformsBindGroup);

    queue.writeBuffer(
      this.globalsBuffer,
      DebugPipelineGlobalUniforms.viewProjectionMatrixOffset,
      projectionViewMatrix,
    );
    queue.writeBuffer(
      this.globalsBuffer,
      DebugPipelineGlobalUniforms.colorsOffset,
      //prettier-ignore
      new Float32Array([
          1,0.5,0,1,
          0.2,1.0,0.5,1,
          0.2,1,0.4,1,
          0.4,0.1,0.4,1,
      ]),
    );

    for (let i = 0; i < this.instanceBatchesLen; i++) {
      const batch = this.instanceBatches[i];
      passEncoder.setBindGroup(1, batch.instanceDataBindGroup);
      queue.writeBuffer(
        batch.modelMatricesBuffer,
        DebugPipelineModelMatrixUniforms.modelMatricesOffset,
        batch.modelMatricesData,
        0,
        batch.instanceCount * DebugPipelineModelMatrixUniforms.elementByteSize,
      );
      passEncoder.drawIndexed(this.indexCount, batch.instanceCount);
    }
  }

  endFrame() {
    this.instanceBatchesLen = 0;
  }
}
