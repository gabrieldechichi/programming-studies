export async function loadFile(path: string): Promise<string> {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`Failed to load file at ${path}`);
  }
  return await response.text();
}
