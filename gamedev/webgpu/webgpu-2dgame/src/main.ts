import { vec2 } from "gl-matrix";
import {
  GPUUniformBuffer,
  MAT4_BYTE_LENGTH,
  createUniformBuffer,
} from "./rendering/bufferUtils";
import { Camera } from "./camera";
import { Content, Sprite } from "./content";
import { SpriteRenderer } from "./spriteRenderer";
import { DebugRenderer } from "./debugPipeline";
import { MathUtils } from "./math/math";

class Renderer {
  private context!: GPUCanvasContext;
  canvas!: HTMLCanvasElement;
  private device!: GPUDevice;
  private projectionViewBuffer!: GPUUniformBuffer;
  private uiProjectionBuffer!: GPUUniformBuffer;
  spriteRenderer!: SpriteRenderer;
  uiRenderer!: SpriteRenderer;

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

    this.projectionViewBuffer = createUniformBuffer(
      this.device,
      MAT4_BYTE_LENGTH,
    );
    this.uiProjectionBuffer = createUniformBuffer(
      this.device,
      MAT4_BYTE_LENGTH,
    );

    this.spriteRenderer = SpriteRenderer.create(
      this.device,
      this.projectionViewBuffer,
      SpriteRenderer.SpriteCenteredGeo,
    );

    this.uiRenderer = SpriteRenderer.create(
      this.device,
      this.uiProjectionBuffer,
      SpriteRenderer.SpriteUIGeo,
    );

    this.debugRenderer = DebugRenderer.create(
      this.device,
      this.projectionViewBuffer,
      DebugRenderer.SpriteCenteredGeo,
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

      this.device.queue.writeBuffer(
        this.projectionViewBuffer,
        0,
        camera.viewProjection as Float32Array,
      );
      this.device.queue.writeBuffer(
        this.uiProjectionBuffer,
        0,
        camera.uiProjection as Float32Array,
      );

      this.spriteRenderer.startFrame();
      this.uiRenderer.startFrame();

      this.debugRenderer.startFrame();

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

      renderEntities();

      this.spriteRenderer.endFrame(passEncoder);
      this.uiRenderer.endFrame(passEncoder);
      this.debugRenderer.endFrame(passEncoder);

      passEncoder.end();

      this.device.queue.submit([commandEncoder.finish()]);
    }
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

type Circle = { center: vec2; radius: number };

class Collision {
  static circleColliderFromSprite(sprite: Sprite): Circle {
    return {
      radius: Math.min(sprite.wh[0], sprite.wh[1]) / 2,
      center: [0, 0],
    };
  }

  static circleCircleCollision(
    a: Circle,
    b: Circle,
  ): { isColliding: boolean; penetration: vec2 } {
    var aToB: vec2 = [b.center[0] - a.center[0], b.center[1] - a.center[1]];
    var aToBSqrdLen = vec2.squaredLength(aToB);
    var radiusSum = a.radius + b.radius;
    if (aToBSqrdLen >= radiusSum * radiusSum) {
      return { isColliding: false, penetration: [0, 0] };
    }

    const aToBLen = Math.sqrt(aToBSqrdLen);
    const delta = radiusSum - aToBLen;
    vec2.normalize(aToB, aToB);
    return { isColliding: true, penetration: vec2.scale(aToB, aToB, -delta) };
  }
}

class Player {
  sprite!: Sprite;
  size!: vec2;
  collider!: Circle;

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

    this.velocity = MathUtils.moveTowards(
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

  worldCollider() {
    return {
      center: this.pos,
      radius: this.collider.radius,
    } as Circle;
  }
}

class Asteroid {
  sprite!: Sprite;
  collider!: Circle;
  pos: vec2 = [0, 0];
  rot: number = 0;

  worldCollider() {
    return {
      center: this.pos,
      radius: this.collider.radius,
    } as Circle;
  }
}

class AsteroidWave {
  asteroids: Asteroid[] = [];

  update(player: Player) {
    const playerCollider = player.worldCollider();
    for (let i = this.asteroids.length - 1; i >= 0; i--) {
      const asteroid = this.asteroids[i];
      const asteroidCollider = asteroid.worldCollider();
      const { isColliding } = Collision.circleCircleCollision(
        playerCollider,
        asteroidCollider,
      );
      if (isColliding) {
        this.asteroids.splice(i, 1);
      }
    }
  }
}

class Color {
  static RED: GPUColorDict = { r: 1, g: 0, b: 0, a: 1 };
  static GREEN: GPUColorDict = { r: 0, g: 0.7, b: 0, a: 1 };
  static BLUE: GPUColorDict = { r: 0, g: 0, b: 1, a: 1 };
}

class Engine {
  camera!: Camera;
  renderer!: Renderer;
  input!: Input;
  time!: Time;
  player!: Player;
  asteroidWave!: AsteroidWave;

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
    //new player
    {
      this.player = new Player();
      this.player.sprite = Content.playerSprite;
      this.player.collider = Collision.circleColliderFromSprite(
        this.player.sprite,
      );
      this.player.size = this.player.sprite.wh;
      this.player.pos[1] = -this.camera.height / 2 + this.player.sprite.wh[1];
    }

    //new asteroid
    {
      this.asteroidWave = new AsteroidWave();
      const asteroid = new Asteroid();
      asteroid.sprite = Content.spriteSheet["meteorBrown_big1"];
      asteroid.collider = Collision.circleColliderFromSprite(asteroid.sprite);
      this.asteroidWave.asteroids.push(asteroid);
    }
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

    this.asteroidWave.update(this.player);

    this.camera.update();
    this.renderer.render(this.camera, () => {
      this.renderer.spriteRenderer.drawSprite(
        this.player.sprite,
        this.player.pos,
      );
      for (const asteroid of this.asteroidWave.asteroids) {
        this.renderer.spriteRenderer.drawSprite(
          asteroid.sprite,
          asteroid.pos,
          asteroid.rot,
        );
      }

      this.renderer.uiRenderer.drawText(
        "Hello World",
        [0, 0],
        Content.defaultFont,
      );
    });
    window.requestAnimationFrame(() => this.loop());
  }
}

async function main() {
  const engine = await Engine.create();
  engine.play();
}

main().then(() => console.log("done"));
