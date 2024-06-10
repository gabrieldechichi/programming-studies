export class Texture {
    id: string;
  texture: GPUTexture;
  sampler: GPUSampler;

  constructor(texture: GPUTexture, sampler: GPUSampler) {
    (this.texture = texture), (this.sampler = sampler);
  }

  static async createTexture(
    device: GPUDevice,
    image: HTMLImageElement,
  ): Promise<Texture> {
    const tex = device.createTexture({
      size: { width: image.width, height: image.height },
      format: "rgba8unorm",
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
      minFilter: "linear",
      magFilter: "linear",
    });

    return new Texture(tex, sampler);
  }

  static async createTextureFromUrl(
    device: GPUDevice,
    imageUrl: string,
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
    return await this.createTexture(device, img);
  }
}
