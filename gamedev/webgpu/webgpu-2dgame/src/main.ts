import { vec2 } from "gl-matrix";
import {
  GPUUniformBuffer,
  MAT4_BYTE_LENGTH,
  createUniformBuffer,
} from "./bufferUtils";
import { Camera } from "./camera";
import { Content, Sprite } from "./content";
import { SpriteRenderer } from "./spriteRenderer";

class Renderer {
  private context!: GPUCanvasContext;
  canvas!: HTMLCanvasElement;
  private device!: GPUDevice;
  private projectionViewBuffer!: GPUUniformBuffer;
  spriteRenderer!: SpriteRenderer;

  public async initialize() {
    const canvas = document.getElementById("canvas") as HTMLCanvasElement;
    this.canvas = canvas;

    const ctx = canvas.getContext("webgpu");
    if (!ctx) {
      alert("WebGPU not supported!");
      return;
    }
    this.context = ctx;

    const adapter = await navigator.gpu.requestAdapter({
      powerPreference: "low-power",
    });

    if (!adapter) {
      alert("adapter not found");
      return;
    }

    this.device = await adapter.requestDevice();

    this.context.configure({
      device: this.device,
      format: navigator.gpu.getPreferredCanvasFormat(),
    });

    {
      this.projectionViewBuffer = createUniformBuffer(
        this.device,
        MAT4_BYTE_LENGTH,
      );
    }

    this.spriteRenderer = SpriteRenderer.create(
      this.device,
      this.projectionViewBuffer,
    );

    await Content.initialize(this.device);
  }

  public render(camera: Camera, renderEntities: () => void) {
    //render
    {
      const commandEncoder = this.device.createCommandEncoder();
      const textureViewer = this.context.getCurrentTexture().createView();
      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view: textureViewer,
            clearValue: { r: 0.8, g: 0.8, b: 0.8, a: 1.0 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      };

      this.spriteRenderer.startFrame(camera.viewProjection);

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

      renderEntities();

      this.spriteRenderer.endFrame(passEncoder);

      passEncoder.end();

      this.device.queue.submit([commandEncoder.finish()]);
    }
  }

  renderSprite(sprite: Sprite, pos?: vec2, rot?: number, size?: vec2) {
    pos = pos || [0, 0];
    rot = rot || 0;
    size = size || sprite.wh;

    this.spriteRenderer.render(sprite, {
      pos,
      rot,
      size,
    });
  }
}

type Time = {
  now: number;
  dt: number;
};

class Input {
  private keyDown: { [key: string]: boolean } = {};

  constructor() {
    window.addEventListener("keydown", (e) => (this.keyDown[e.key] = true));
    window.addEventListener("keyup", (e) => (this.keyDown[e.key] = false));
  }

  public isKeyDown(key: string): boolean {
    return this.keyDown[key];
  }

  public isKeyUp(key: string): boolean {
    return !this.keyDown[key];
  }
}

class Player {
  pos: vec2 = [0, 0];
  sprite!: Sprite;
  speed: number = 2;

  update(dt: number, input: Input) {
    const moveInput = vec2.create();
    if (input.isKeyDown("d")) {
      moveInput[0] += 1;
    }
    if (input.isKeyDown("a")) {
      moveInput[0] -= 1;
    }
    if (input.isKeyDown("w")) {
      moveInput[1] += 1;
    }
    if (input.isKeyDown("s")) {
      moveInput[1] -= 1;
    }
    vec2.normalize(moveInput, moveInput);

    this.pos[0] += moveInput[0] * dt;
    this.pos[1] += moveInput[1] * dt;
  }
}

class Engine {
  private camera!: Camera;
  private renderer!: Renderer;
  input!: Input;
  time!: Time;
  private player!: Player;

  static async create(): Promise<Engine> {
    const engine = new Engine();
    engine.renderer = new Renderer();
    await engine.renderer.initialize();
    engine.camera = new Camera(
      engine.renderer.canvas.width,
      engine.renderer.canvas.height,
    );

    engine.input = new Input();
    engine.time = { now: performance.now(), dt: 0 };
    return engine;
  }

  play() {
    this.player = new Player();
    this.player.sprite = Content.playerSprite;
    this.player.pos[1] = -this.camera.height / 2 + this.player.sprite.wh[1];
    this.loop();
  }

  loop() {
    const now = performance.now();
    this.time.dt = now - this.time.now;
    this.time.now = now;

    this.player.update(this.time.dt, this.input);
    this.camera.update();
    this.renderer.render(this.camera, () => {
      this.renderer.renderSprite(this.player.sprite, this.player.pos);
    });
    window.requestAnimationFrame(() => this.loop());
  }
}

async function main() {
  const engine = await Engine.create();
  engine.play();
}

main().then(() => console.log("done"));
