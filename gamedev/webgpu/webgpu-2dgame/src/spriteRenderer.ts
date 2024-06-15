import { mat4 } from "gl-matrix";
import { GPUUniformBuffer } from "./rendering/bufferUtils";
import { SpritePipeline } from "./spritePipeline";
import { Sprite } from "./content";
import { MathUtils, Transform } from "./math/math";
import { InstanceData } from "./rendering/instancing";

const MAX_INSTANCES = 1024;

export class SpriteRenderer {
  device!: GPUDevice;
  projectionViewBuffer!: GPUUniformBuffer;
  instancesPerTexture: { [id: string]: InstanceData } = {};
  pipelinesPerTexture: { [id: string]: SpritePipeline } = {};

  static create(
    device: GPUDevice,
    projectionViewBuffer: GPUUniformBuffer,
  ): SpriteRenderer {
    const renderer = new SpriteRenderer();
    renderer.device = device;
    renderer.projectionViewBuffer = projectionViewBuffer;
    return renderer;
  }

  startFrame(projectionViewMatrix: mat4) {
    for (const key in this.instancesPerTexture) {
      this.instancesPerTexture[key].count = 0;
    }

    //TODO: duplicate writes to projection buffer (move to main renderer)
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

    //get or create instanceData
    let instanceData = this.instancesPerTexture[texture.id];
    if (!instanceData) {
      instanceData = InstanceData.create(
        this.device,
        MAX_INSTANCES,
        SpritePipeline.VERTEX_INSTANCE_FLOAT_NUM,
        //prettier-ignore
        new Float32Array([
            //pos         //uv
            -0.5, -0.5,   0.0, 1.0,
             0.5, -0.5,   1.0, 1.0,
             0.5, 0.5,    1.0, 0.0,
            -0.5, 0.5,    0.0, 0.0
        ]),
        new Int16Array([0, 1, 2, 2, 3, 0]),
      );

      this.instancesPerTexture[texture.id] = instanceData;
    }

    //TODO: handle requests above MAX_INSTANCES
    const offset = instanceData.count * instanceData.floatStride;

    const modelMatrix = MathUtils.trs(transform);

    for (let i = 0; i < 16; i++) {
      instanceData.data[offset + i] = modelMatrix[i];
    }

    const uvOffset = [
      sprite.xy[0] / sprite.texture.size[0],
      sprite.xy[1] / sprite.texture.size[1],
    ];
    const uvScale = [
      sprite.wh[0] / sprite.texture.size[0],
      sprite.wh[1] / sprite.texture.size[1],
    ];
    instanceData.data[offset + 16 + 0] = uvOffset[0];
    instanceData.data[offset + 16 + 1] = uvOffset[1];
    instanceData.data[offset + 16 + 2] = uvScale[0];
    instanceData.data[offset + 16 + 3] = uvScale[1];

    instanceData.count++;
  }

  endFrame(passEncoder: GPURenderPassEncoder) {
    for (const textureId in this.instancesPerTexture) {
      const instanceData = this.instancesPerTexture[textureId];
      const pipeline = this.pipelinesPerTexture[textureId];

      if (instanceData.count > 0) {
        this.device.queue.writeBuffer(
          instanceData.instancesBuffer,
          0,
          instanceData.data,
        );

        passEncoder.setPipeline(pipeline.pipeline);
        passEncoder.setBindGroup(0, pipeline.projectionViewBindGroup);
        passEncoder.setBindGroup(1, pipeline.textureBindGroup);

        passEncoder.setIndexBuffer(instanceData.indexBuffer, "uint16");
        passEncoder.setVertexBuffer(0, instanceData.geometryBuffer);
        passEncoder.setVertexBuffer(1, instanceData.instancesBuffer);

        passEncoder.drawIndexed(instanceData.indexCount, instanceData.count);
      }
    }
  }
}
