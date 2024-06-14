import { vec2 } from "gl-matrix";
import {
  GPUUniformBuffer,
  MAT4_BYTE_LENGTH,
  createUniformBuffer,
} from "./bufferUtils";
import { Camera } from "./camera";
import { Content, Sprite } from "./content";
import { SpriteRenderer } from "./spriteRenderer";
import { DebugRenderer } from "./debugPipeline";

class Renderer {
  private context!: GPUCanvasContext;
  canvas!: HTMLCanvasElement;
  private device!: GPUDevice;
  private projectionViewBuffer!: GPUUniformBuffer;
  spriteRenderer!: SpriteRenderer;
  debugRenderer!: DebugRenderer;

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

    this.debugRenderer = DebugRenderer.create(
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
      this.debugRenderer.startFrame(camera.viewProjection);

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

      renderEntities();

      this.spriteRenderer.endFrame(passEncoder);
      this.debugRenderer.endFrame(passEncoder);

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
  sprite!: Sprite;
  size!: vec2;

  pos: vec2 = [0, 0];
  velocity: vec2 = [0, 0];

  maxSpeed: number = 0.5;
  acceleration: number = 0.005;

  update(dt: number, input: Input, canvasSize: vec2) {
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

    const targetVelocity = vec2.scale(vec2.create(), moveInput, this.maxSpeed);

    this.velocity = moveTowards(
      this.velocity,
      targetVelocity,
      this.acceleration * dt,
    );

    this.pos[0] += this.velocity[0] * dt;
    this.pos[1] += this.velocity[1] * dt;

    const canvasExtents = [canvasSize[0] / 2, canvasSize[1] / 2];
    const playerExtents = [this.size[0] / 2, this.size[1] / 2];
    if (this.pos[0] - playerExtents[0] < -canvasExtents[0]) {
      this.pos[0] = -canvasExtents[0] + playerExtents[0];
    } else if (this.pos[0] + playerExtents[0] > canvasExtents[0]) {
      this.pos[0] = canvasExtents[0] - playerExtents[0];
    }

    if (this.pos[1] - playerExtents[1] < -canvasExtents[1]) {
      this.pos[1] = -canvasExtents[1] + playerExtents[1];
    } else if (this.pos[1] + playerExtents[1] > canvasExtents[1]) {
      this.pos[1] = canvasExtents[1] - playerExtents[1];
    }
  }
}

function moveTowards(
  current: vec2,
  target: vec2,
  maxDistanceDelta: number,
): vec2 {
  const toTarget: vec2 = [target[0] - current[0], target[1] - current[1]];
  const magnitude = vec2.len(toTarget);
  if (magnitude <= maxDistanceDelta || magnitude == 0) {
    return target;
  }
  const dx = (toTarget[0] / magnitude) * maxDistanceDelta;
  const dy = (toTarget[1] / magnitude) * maxDistanceDelta;

  return [current[0] + dx, current[1] + dy] as vec2;
}

class Engine {
  camera!: Camera;
  renderer!: Renderer;
  input!: Input;
  time!: Time;
  player!: Player;

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
    this.player.size = this.player.sprite.wh;
    this.player.pos[1] = -this.camera.height / 2 + this.player.sprite.wh[1];
    this.loop();
  }

  loop() {
    const now = performance.now();
    this.time.dt = now - this.time.now;
    this.time.now = now;

    this.player.update(this.time.dt, this.input, [
      this.renderer.canvas.width,
      this.renderer.canvas.height,
    ]);
    this.camera.update();
    this.renderer.render(this.camera, () => {
      this.renderer.renderSprite(this.player.sprite, this.player.pos);

      this.renderer.debugRenderer.drawWireSquare(
        {
          pos: this.player.pos,
          rot: 0,
          size: this.player.size,
        },
        2,
      );

      this.renderer.debugRenderer.drawSquare(
        {
          pos: [-200, -400],
          rot: Math.PI/4 - 0.2,
          size: [50, 50]
        }
      );

      this.renderer.debugRenderer.drawWireSquare(
        {
          pos: [0, -200],
          rot: 0,
          size: this.player.size,
        },
        2,
        { r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
      );
      this.renderer.debugRenderer.drawWireSquare(
        {
          pos: [100, -200],
          rot: 0,
          size: this.player.size,
        },
        2,
      );

      this.renderer.debugRenderer.drawCircle([100, -200], 50);
    });
    window.requestAnimationFrame(() => this.loop());
  }
}

async function main() {
  const engine = await Engine.create();
  engine.play();
}

main().then(() => console.log("done"));
