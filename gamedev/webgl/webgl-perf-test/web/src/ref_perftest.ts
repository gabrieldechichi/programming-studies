import { mat4, vec2, vec4 } from "gl-matrix";
import * as Time from "./game/time";
import { Pivot, SpriteRenderer } from "./game/sprite-renderer";
import { resizeCanvas2, Viewport } from "./game/viewport";
import * as graphics from "src/engine/graphics";

const BATCH_SIZE = 1024 * 1;
const MAX_INSTANCES = BATCH_SIZE * 1 - 10;
const MAX_SPEED = 200;

interface Ball {
  position: vec2;
  radius: number;
  velocity: vec2;
  colorIndex: number;
}

let colors: vec4[] = [
  [1, 0, 0, 1],
  [0, 1, 0, 1],
  [0, 0, 1, 1],
  [1, 1, 0, 1],
  [1, 0, 1, 1],
  [0, 1, 1, 1],
];

let time: Time.Time;
let perf: Time.Performance;
let viewProjection: mat4;
let balls: Ball[] = [];
let canvas: HTMLCanvasElement;
let gl: WebGL2RenderingContext;
let viewport: Viewport;
let spriteRenderer: SpriteRenderer;
let font: graphics.Font;
let whiteTex: WebGLTexture;
let ballSprite: graphics.Sprite;

export async function run() {
  canvas = document.getElementById("canvas") as HTMLCanvasElement;

  const glCtx = canvas.getContext("webgl2");
  if (!glCtx) {
    return;
  }
  gl = glCtx;

  spriteRenderer = SpriteRenderer.new(gl, BATCH_SIZE);

  const fontTex = await spriteRenderer.addAtlas("/assets/fonts/spritefont.png");
  font = await graphics.loadFontWithTexture(
    fontTex!,
    "/assets/fonts/spritefont.xml",
  );

  whiteTex = spriteRenderer.addTexturePixels(
    new Uint8Array([255, 255, 255, 255]),
    1,
    1,
  )!;
  ballSprite = { texture: whiteTex, x: 0, y: 0, w: 1, h: 1 };

  viewport = {} as Viewport;

  resizeCanvas2(canvas, gl, viewport);
  window.addEventListener("resize", () => resizeCanvas2(canvas, gl, viewport));
  window.addEventListener("load", () => resizeCanvas2(canvas, gl, viewport));

  const { width, height } = viewport;

  viewProjection = mat4.create();
  mat4.ortho(
    viewProjection,
    -width / 2,
    width / 2,
    -height / 2,
    height / 2,
    -1,
    10,
  );

  time = Time.initialize();
  perf = Time.initializePerf();

  balls = Array.from({ length: MAX_INSTANCES }, () => {
    const x = randomFloat(-width / 2, width / 2);
    const y = randomFloat(-height / 2, height / 2);
    const vx = randomFloat(-1, 1) * MAX_SPEED;
    const vy = randomFloat(-1, 1) * MAX_SPEED;

    return {
      position: [x, y],
      radius: 5,
      velocity: [vx, vy],
      colorIndex: randomInt(0, colors.length - 1),
    };
  });

  updateInternal();
}

function updateInternal() {
  const result = update();
  if (result) {
    requestAnimationFrame(updateInternal);
  }
}

function randomFloat(min: number, max: number): number {
  return Math.random() * (max - min) + min;
}

function randomInt(min: number, max: number): number {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

function update(): boolean {
  Time.update(time);
  Time.updatePerf(perf, time);
  const dt = time.dt;
  const { width, height } = viewport;
  for (const ball of balls) {
    ball.position[0] += ball.velocity[0] * dt;
    ball.position[1] += ball.velocity[1] * dt;

    if (ball.position[0] + ball.radius > width / 2 && ball.velocity[0] > 0) {
      ball.velocity[0] *= -1;
    } else if (
      ball.position[0] - ball.radius < -width / 2 &&
      ball.velocity[0] < 0
    ) {
      ball.velocity[0] *= -1;
    }

    if (ball.position[1] + ball.radius > height / 2 && ball.velocity[1] > 0) {
      ball.velocity[1] *= -1;
    } else if (
      ball.position[1] - ball.radius < -height / 2 &&
      ball.velocity[1] < 0
    ) {
      ball.velocity[1] *= -1;
    }
  }

  for (const ball of balls) {
    spriteRenderer.drawSprite({
      sprite: ballSprite,
      pos: [ball.position[0], ball.position[1], 0],
      scale: [ball.radius, ball.radius],
      color: colors[ball.colorIndex],
    });
  }

  spriteRenderer.drawText({
    text: `entity coun:${balls.length}\nfps: ${(1 / perf.dtAvg).toFixed(1)}\ndt: ${(perf.dtAvg * 1000).toFixed(1)}ms`,
    topLeft: [-viewport.width / 2 + 5, viewport.height / 2 - 5],
    font,
    size: 8,
  });

  spriteRenderer.drawSprite({
    sprite: ballSprite,
    pos: [-width / 2, height / 2, 0.6],
    scale: [60, 32],
    color: [0.1, 0.1, 0.1, 0.8],
    pivot: Pivot.TOP_LEFT,
  });

  spriteRenderer.render(viewport.viewProjectionMatrix);
  spriteRenderer.endFrame();

  return true;
}
