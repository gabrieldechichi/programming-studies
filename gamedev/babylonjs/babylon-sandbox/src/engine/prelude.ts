export * from "./animations";

export function enforceAspectRatio(
  canvas: HTMLCanvasElement,
  aspectRatio: number,
) {
  const width = window.innerWidth;
  const height = width / aspectRatio;
  canvas.width = width;
  canvas.height = height;
}

export function waitForMs(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
