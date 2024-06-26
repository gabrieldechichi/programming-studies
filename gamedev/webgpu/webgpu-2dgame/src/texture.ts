import { vec2 } from "gl-matrix";

export class Texture {
  id!: string;
  texture: GPUTexture;
  sampler: GPUSampler;
  size: vec2;

  constructor(
    texture: GPUTexture,
    sampler: GPUSampler,
    id: string,
    size: vec2,
  ) {
    this.texture = texture;
    this.sampler = sampler;
    this.id = id;
    this.size = size;
  }

  static async createTexture(
    device: GPUDevice,
    image: HTMLImageElement,
    filterMode: GPUFilterMode = "nearest",
  ): Promise<Texture> {
    const tex = device.createTexture({
      size: { width: image.width, height: image.height },
      format: "rgba8unorm",
      // format: navigator.gpu.getPreferredCanvasFormat(),
      usage:
        GPUTextureUsage.COPY_DST |
        GPUTextureUsage.TEXTURE_BINDING |
        GPUTextureUsage.RENDER_ATTACHMENT,
    });
    const data = await createImageBitmap(image);
    device.queue.copyExternalImageToTexture(
      { source: data },
      { texture: tex },
      { width: image.width, height: image.height },
    );

    const sampler = device.createSampler({
      minFilter: filterMode,
      magFilter: filterMode,
    });

    return new Texture(tex, sampler, image.src, [image.width, image.height]);
  }

  static async createEmptyTexture(
    device: GPUDevice,
    width: number,
    height: number,
    filterMode: GPUFilterMode = "nearest",
  ) {
    const tex = device.createTexture({
      size: { width: width, height: height },
      //format: "rgba8unorm",
      format: navigator.gpu.getPreferredCanvasFormat(),
      usage:
        GPUTextureUsage.COPY_DST |
        GPUTextureUsage.TEXTURE_BINDING |
        GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const sampler = device.createSampler({
      minFilter: filterMode,
      magFilter: filterMode,
    });

    return new Texture(tex, sampler, "empty", [width, height]);
  }

  static async createTextureFromUrl(
    device: GPUDevice,
    imageUrl: string,
    filterMode: GPUFilterMode = "nearest",
  ): Promise<Texture> {
    const loadImage = new Promise<HTMLImageElement>((resolve, reject) => {
      const image = new Image();
      image.src = imageUrl;
      image.onload = () => resolve(image);
      image.onerror = () => {
        console.error(`Failed to load image ${imageUrl}`);
        reject();
      };
    });
    const img = await loadImage;
    return await this.createTexture(device, img, filterMode);
  }
}
