export class WasmMemoryInterface {
  public memory: WebAssembly.Memory;
  private intSize: number;

  constructor(memory: WebAssembly.Memory) {
    this.memory = memory;
    // Size (in bytes) of the integer type, should be 4 on `js_wasm32` and 8 on `js_wasm64p32`
    this.intSize = 4;
  }

  get mem(): DataView {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new DataView(this.memory.buffer);
  }

  loadF32Array(addr: number, len: number): Float32Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Float32Array(this.memory.buffer, addr, len);
  }

  loadF64Array(addr: number, len: number): Float64Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Float64Array(this.memory.buffer, addr, len);
  }

  loadU32Array(addr: number, len: number): Uint32Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Uint32Array(this.memory.buffer, addr, len);
  }

  loadI32Array(addr: number, len: number): Int32Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Int32Array(this.memory.buffer, addr, len);
  }

  loadI16Array(addr: number, len: number): Int16Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Int16Array(this.memory.buffer, addr, len);
  }

  loadU8Array(addr: number, len: number): Uint8Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Uint8Array(this.memory.buffer, addr, len);
  }

  loadU8(addr: number): number {
    return this.mem.getUint8(addr);
  }
  loadI8(addr: number): number {
    return this.mem.getInt8(addr);
  }
  loadU16(addr: number): number {
    return this.mem.getUint16(addr, true);
  }
  loadI16(addr: number): number {
    return this.mem.getInt16(addr, true);
  }
  loadU32(addr: number): number {
    return this.mem.getUint32(addr, true);
  }
  loadI32(addr: number): number {
    return this.mem.getInt32(addr, true);
  }
  loadU64(addr: number): number {
    const lo = this.mem.getUint32(addr + 0, true);
    const hi = this.mem.getUint32(addr + 4, true);
    return lo + hi * 4294967296;
  }
  loadI64(addr: number): number {
    const lo = this.mem.getUint32(addr + 0, true);
    const hi = this.mem.getInt32(addr + 4, true);
    return lo + hi * 4294967296;
  }
  loadF32(addr: number): number {
    return this.mem.getFloat32(addr, true);
  }
  loadF64(addr: number): number {
    return this.mem.getFloat64(addr, true);
  }
  loadInt(addr: number): number {
    if (this.intSize === 8) {
      return this.loadI64(addr);
    } else if (this.intSize === 4) {
      return this.loadI32(addr);
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }
  loadUint(addr: number): number {
    if (this.intSize === 8) {
      return this.loadU64(addr);
    } else if (this.intSize === 4) {
      return this.loadU32(addr);
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }
  loadPtr(addr: number): number {
    return this.loadU32(addr);
  }

  loadB32(addr: number): boolean {
    return this.loadU32(addr) !== 0;
  }

  loadBytes(ptr: number, len: number | bigint): Uint8Array {
    if (!this.memory) {
      throw new Error("Memory not set");
    }
    return new Uint8Array(this.memory.buffer, ptr, Number(len));
  }

  loadString(ptr: number, len: number | bigint): string {
    const bytes = this.loadBytes(ptr, Number(len));
    return new TextDecoder().decode(bytes);
  }

  loadCstring(ptr: number): string | null {
    return this.loadCstringDirect(this.loadPtr(ptr));
  }

  loadCstringDirect(start: number): string | null {
    if (start === 0) {
      return null;
    }
    let len = 0;
    for (; this.mem.getUint8(start + len) !== 0; len += 1) {}
    return this.loadString(start, len);
  }

  storeU8(addr: number, value: number): void {
    this.mem.setUint8(addr, value);
  }
  storeI8(addr: number, value: number): void {
    this.mem.setInt8(addr, value);
  }
  storeU16(addr: number, value: number): void {
    this.mem.setUint16(addr, value, true);
  }
  storeI16(addr: number, value: number): void {
    this.mem.setInt16(addr, value, true);
  }
  storeU32(addr: number, value: number): void {
    this.mem.setUint32(addr, value, true);
  }
  storeI32(addr: number, value: number): void {
    this.mem.setInt32(addr, value, true);
  }
  storeU64(addr: number, value: number | bigint): void {
    this.mem.setUint32(addr + 0, Number(value), true);

    let div: number | bigint = 4294967296;
    if (typeof value === "bigint") {
      div = BigInt(div);
    }

    this.mem.setUint32(addr + 4, Math.floor(Number(value / div)), true);
  }
  storeI64(addr: number, value: number | bigint): void {
    this.mem.setUint32(addr + 0, Number(value), true);

    let div: number | bigint = 4294967296;
    if (typeof value === "bigint") {
      div = BigInt(div);
    }

    this.mem.setInt32(addr + 4, Math.floor(Number(value / div)), true);
  }
  storeF32(addr: number, value: number): void {
    this.mem.setFloat32(addr, value, true);
  }
  storeF64(addr: number, value: number): void {
    this.mem.setFloat64(addr, value, true);
  }
  storeInt(addr: number, value: number | bigint): void {
    if (this.intSize === 8) {
      this.storeI64(addr, value);
    } else if (this.intSize === 4) {
      this.storeI32(addr, Number(value));
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }
  storeUint(addr: number, value: number | bigint): void {
    if (this.intSize === 8) {
      this.storeU64(addr, value);
    } else if (this.intSize === 4) {
      this.storeU32(addr, Number(value));
    } else {
      throw new Error("Unhandled `intSize`, expected `4` or `8`");
    }
  }

  // Returned length might not be the same as `value.length` if non-ascii strings are given.
  storeString(addr: number, value: string): number {
    const src = new TextEncoder().encode(value);
    const dst = new Uint8Array(this.memory.buffer, addr, src.length);
    dst.set(src);
    return src.length;
  }
}
