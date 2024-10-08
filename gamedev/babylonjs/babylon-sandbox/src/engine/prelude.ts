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

export async function loadFile(url: string): Promise<string> {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to load file: ${response.statusText}`);
  }
  return await response.text();
}

export async function loadJson<T>(url: string): Promise<T> {
  const str = await loadFile(url);
  return JSON.parse(str) as T;
}
