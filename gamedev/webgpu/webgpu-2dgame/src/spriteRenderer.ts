import { mat4, vec2 } from "gl-matrix";
import {
  GPUIndexBuffer,
  GPUUniformBuffer,
  GPUVertexBuffer,
  createIndexBuffer,
  createVertexBuffer,
} from "./bufferUtils";
import { SpritePipeline } from "./spritePipeline";
import { Sprite } from "./content";

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

type Transform = {
  pos: vec2;
  rot: number;
  size: vec2;
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

  render(sprite: Sprite, transform: Transform) {
    //get or create pipeline
    const texture = sprite.texture;
    let pipeline = this.pipelinesPerTexture[texture.id];
    if (!pipeline) {
      pipeline = SpritePipeline.create(
        this.device,
        texture,
        this.projectionViewBuffer,
      );
      this.pipelinesPerTexture[texture.id] = pipeline;
    }

    //get or create batch
    let batches = this.batchesPerTexture[texture.id];
    if (!batches) {
      batches = [];
      this.batchesPerTexture[texture.id] = batches;
    }
    let batch = batches[batches.length - 1];
    if (!batch) {
      batch = new SpriteBatch();
      batch.instanceCount = 0;
      batches.push(batch);
    }

    //set vertex pos
    const spriteBatchIndex = batch.instanceCount;
    const pos = transform.pos;
    const rot = transform.rot;
    const e = [transform.size[0] * 0.5, transform.size[1] * 0.5];

    const vs: vec2[] = [
      this.rotateVertex([pos[0] - e[0], pos[1] - e[1]], pos, rot),
      this.rotateVertex([pos[0] + e[0], pos[1] - e[1]], pos, rot),
      this.rotateVertex([pos[0] + e[0], pos[1] + e[1]], pos, rot),
      this.rotateVertex([pos[0] - e[0], pos[1] + e[1]], pos, rot),
    ];

    const u0 = sprite.xy[0] / sprite.texture.size[0];
    const v0 = sprite.xy[1] / sprite.texture.size[1];
    const u1 = (sprite.xy[0] + sprite.wh[0]) / sprite.texture.size[0];
    const v1 = (sprite.xy[1] + sprite.wh[1]) / sprite.texture.size[1];
    const uvs: vec2[] = [
      [u0, v1],
      [u1, v1],
      [u1, v0],
      [u0, v0],
    ];

    //prettier-ignore
    const vertices = [
      // xy                 //uv
      vs[0][0], vs[0][1],   uvs[0][0], uvs[0][1],
      vs[1][0], vs[1][1],   uvs[1][0], uvs[1][1],
      vs[2][0], vs[2][1],   uvs[2][0], uvs[2][1],
      vs[3][0], vs[3][1],   uvs[3][0], uvs[3][1],
    ];

    const vertOffset = spriteBatchIndex * VERTEX_BUFFER_FLOATS_PER_SPRITE;
    for (let i = 0; i < vertices.length; i++) {
      batch.vertexData[i + vertOffset] = vertices[i];
    }

    batch.instanceCount++;

    if (batch.instanceCount >= MAX_SPRITE_PER_BATCH) {
      const newBatch = new SpriteBatch();
      newBatch.instanceCount = 0;
      batches.push(newBatch);
    }
  }

  rotateVertex(v: vec2, origin: vec2, rot: number): vec2 {
    return vec2.rotate(vec2.create(), v, origin, rot);
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
}
