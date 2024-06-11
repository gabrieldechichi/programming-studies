import { mat4, vec2 } from "gl-matrix";
import {
  GPUIndexBuffer,
  GPUUniformBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";
import { SpritePipeline } from "./spritePipeline";
import { Texture } from "./texture";

const MAX_SPRITE_PER_BATCH = 1024;
const INDICEX_PER_SPRITE = 6; //quad
const VERTEX_PER_SPRITE = 4; //quad

const VERTEX_BUFFER_FLOATS_PER_SPRITE =
  VERTEX_PER_SPRITE * SpritePipeline.FLOATS_PER_VERTEX;

class SpriteBatch {
  vertexData = new Float32Array(
    VERTEX_BUFFER_FLOATS_PER_SPRITE * MAX_SPRITE_PER_BATCH,
  );
  instanceCount: number = 0;
}

type SpriteVertexBuffers = {
  vertexBuffer: GPUVertexBuffer;
};

export class SpriteRenderer {
  device!: GPUDevice;
  projectionViewBuffer!: GPUUniformBuffer;
  indexBuffer!: GPUIndexBuffer;
  batchesPerTexture: { [id: string]: Array<SpriteBatch> } = {};
  pipelinesPerTexture: { [id: string]: SpritePipeline } = {};
  vertexBuffersPool: Array<SpriteVertexBuffers> = [];

  static create(
    device: GPUDevice,
    projectionViewBuffer: GPUUniformBuffer,
  ): SpriteRenderer {
    const renderer = new SpriteRenderer();
    renderer.device = device;
    renderer.projectionViewBuffer = projectionViewBuffer;
    const indices = new Int16Array(INDICEX_PER_SPRITE * MAX_SPRITE_PER_BATCH);

    for (let i = 0; i < indices.length; i++) {
      indices[i * INDICEX_PER_SPRITE + 0] = 0 + VERTEX_PER_SPRITE * i;
      indices[i * INDICEX_PER_SPRITE + 1] = 1 + VERTEX_PER_SPRITE * i;
      indices[i * INDICEX_PER_SPRITE + 2] = 2 + VERTEX_PER_SPRITE * i;

      indices[i * INDICEX_PER_SPRITE + 3] = 2 + VERTEX_PER_SPRITE * i;
      indices[i * INDICEX_PER_SPRITE + 4] = 3 + VERTEX_PER_SPRITE * i;
      indices[i * INDICEX_PER_SPRITE + 5] = 0 + VERTEX_PER_SPRITE * i;
    }
    renderer.indexBuffer = createIndexBuffer(renderer.device, indices);
    return renderer;
  }

  startFrame(projectionViewMatrix: mat4) {
    //PERF: stop resetting batches every frame
    this.batchesPerTexture = {};

    this.device.queue.writeBuffer(
      this.projectionViewBuffer,
      0,
      projectionViewMatrix as Float32Array,
    );
  }

  render(texture: Texture, pos: vec2) {
    this.getOrCreatePipeline(texture);
    const batch = this.getAvailableBatchForPipeline(texture);

    this.fillSpriteVertexData(batch, batch.instanceCount, pos, texture.size);
    batch.instanceCount++;
  }

  endFrame(passEncoder: GPURenderPassEncoder) {
    passEncoder.setIndexBuffer(this.indexBuffer, "uint16");
    const usedSpriteBuffers: SpriteVertexBuffers[] = [];

    for (const textureId in this.batchesPerTexture) {
      const batches = this.batchesPerTexture[textureId];
      if (!batches || batches.length == 0) {
        continue;
      }
      const pipeline = this.pipelinesPerTexture[textureId];
      if (!pipeline) {
        continue;
      }

      passEncoder.setPipeline(pipeline.pipeline);
      passEncoder.setBindGroup(0, pipeline.projectionViewBindGroup);
      passEncoder.setBindGroup(1, pipeline.textureBindGroup);

      for (const batch of batches) {
        let spriteBuffers = this.vertexBuffersPool.pop();
        if (!spriteBuffers) {
          spriteBuffers = {
            vertexBuffer: createVertexBuffer(this.device, batch.vertexData),
          } as SpriteVertexBuffers;
        } else {
          //todo: specify write data size?
          this.device.queue.writeBuffer(
            spriteBuffers.vertexBuffer,
            0,
            batch.vertexData,
          );
        }

        usedSpriteBuffers.push(spriteBuffers);

        passEncoder.setVertexBuffer(0, spriteBuffers.vertexBuffer);

        passEncoder.drawIndexed(batch.instanceCount * INDICEX_PER_SPRITE);
      }

      for (const spriteBuffers of usedSpriteBuffers) {
        this.vertexBuffersPool.push(spriteBuffers);
      }
    }
  }

  private fillSpriteVertexData(
    batch: SpriteBatch,
    spriteBatchIndex: number,
    pos: vec2,
    size: vec2,
  ) {
    const e = vec2.scale(vec2.create(), size, 0.5);
    //prettier-ignore
    const vertices = [
      // xy                           //uv         //color
      pos[0] - e[0], pos[1] - e[1],   0.0, 1.0,    1.0, 1.0, 1.0, 1.0,
      pos[0] + e[0], pos[1] - e[1],   1.0, 1.0,    1.0, 1.0, 1.0, 1.0,
      pos[0] + e[0], pos[1] + e[1],   1.0, 0.0,    1.0, 1.0, 1.0, 1.0,
      pos[0] - e[0], pos[1] + e[1],   0.0, 0.0,    1.0, 1.0, 1.0, 1.0,
    ];
    const vertOffset = spriteBatchIndex * VERTEX_BUFFER_FLOATS_PER_SPRITE;
    for (let i = 0; i < vertices.length; i++) {
      batch.vertexData[i + vertOffset] = vertices[i];
    }
  }

  private getOrCreatePipeline(texture: Texture) {
    let pipeline = this.pipelinesPerTexture[texture.id];
    if (!pipeline) {
      pipeline = SpritePipeline.create(
        this.device,
        texture,
        this.projectionViewBuffer,
      );
      this.pipelinesPerTexture[texture.id] = pipeline;
    }
    return pipeline;
  }

  private getAvailableBatchForPipeline(texture: Texture): SpriteBatch {
    let batches = this.batchesPerTexture[texture.id];
    if (!batches) {
      batches = [];
      this.batchesPerTexture[texture.id] = batches;
    }
    let batch = batches[batches.length - 1];
    if (!batch || batch.instanceCount >= MAX_SPRITE_PER_BATCH) {
      batch = new SpriteBatch();
      batch.instanceCount = 0;
      batches.push(batch);
    }
    return batch;
  }
}
