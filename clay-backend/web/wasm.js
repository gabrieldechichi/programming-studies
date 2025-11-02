export class WasmMemoryInterface {
    constructor(memory) {
        this.memory = memory;
        // Size (in bytes) of the integer type, should be 4 on `js_wasm32` and 8 on `js_wasm64p32`
        this.intSize = 4;
    }

    get mem() {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new DataView(this.memory.buffer);
    }

    loadF32Array(addr, len) {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new Float32Array(this.memory.buffer, addr, len);
    }

    loadF64Array(addr, len) {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new Float64Array(this.memory.buffer, addr, len);
    }

    loadU32Array(addr, len) {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new Uint32Array(this.memory.buffer, addr, len);
    }

    loadI32Array(addr, len) {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new Int32Array(this.memory.buffer, addr, len);
    }

    loadI16Array(addr, len) {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new Int16Array(this.memory.buffer, addr, len);
    }

    loadU8(addr) {
        return this.mem.getUint8(addr);
    }
    loadI8(addr) {
        return this.mem.getInt8(addr);
    }
    loadU16(addr) {
        return this.mem.getUint16(addr, true);
    }
    loadI16(addr) {
        return this.mem.getInt16(addr, true);
    }
    loadU32(addr) {
        return this.mem.getUint32(addr, true);
    }
    loadI32(addr) {
        return this.mem.getInt32(addr, true);
    }
    loadU64(addr) {
        const lo = this.mem.getUint32(addr + 0, true);
        const hi = this.mem.getUint32(addr + 4, true);
        return lo + hi * 4294967296;
    }
    loadI64(addr) {
        const lo = this.mem.getUint32(addr + 0, true);
        const hi = this.mem.getInt32(addr + 4, true);
        return lo + hi * 4294967296;
    }
    loadF32(addr) {
        return this.mem.getFloat32(addr, true);
    }
    loadF64(addr) {
        return this.mem.getFloat64(addr, true);
    }
    loadInt(addr) {
        if (this.intSize === 8) {
            return this.loadI64(addr);
        } else if (this.intSize === 4) {
            return this.loadI32(addr);
        } else {
            throw new Error("Unhandled `intSize`, expected `4` or `8`");
        }
    }
    loadUint(addr) {
        if (this.intSize === 8) {
            return this.loadU64(addr);
        } else if (this.intSize === 4) {
            return this.loadU32(addr);
        } else {
            throw new Error("Unhandled `intSize`, expected `4` or `8`");
        }
    }
    loadPtr(addr) {
        return this.loadU32(addr);
    }

    loadB32(addr) {
        return this.loadU32(addr) !== 0;
    }

    loadBytes(ptr, len) {
        if (!this.memory) {
            throw new Error("Memory not set");
        }
        return new Uint8Array(this.memory.buffer, ptr, Number(len));
    }

    loadString(ptr, len) {
        const bytes = this.loadBytes(ptr, Number(len));
        return new TextDecoder().decode(bytes);
    }

    loadCstring(ptr) {
        return this.loadCstringDirect(this.loadPtr(ptr));
    }

    loadCstringDirect(start) {
        if (start === 0) {
            return null;
        }
        let len = 0;
        for (; this.mem.getUint8(start + len) !== 0; len += 1) { }
        return this.loadString(start, len);
    }

    storeU8(addr, value) {
        this.mem.setUint8(addr, value);
    }
    storeI8(addr, value) {
        this.mem.setInt8(addr, value);
    }
    storeU16(addr, value) {
        this.mem.setUint16(addr, value, true);
    }
    storeI16(addr, value) {
        this.mem.setInt16(addr, value, true);
    }
    storeU32(addr, value) {
        this.mem.setUint32(addr, value, true);
    }
    storeI32(addr, value) {
        this.mem.setInt32(addr, value, true);
    }
    storeU64(addr, value) {
        this.mem.setUint32(addr + 0, Number(value), true);

        let div = 4294967296;
        if (typeof value === "bigint") {
            div = BigInt(div);
        }

        this.mem.setUint32(addr + 4, Math.floor(Number(value / div)), true);
    }
    storeI64(addr, value) {
        this.mem.setUint32(addr + 0, Number(value), true);

        let div = 4294967296;
        if (typeof value === "bigint") {
            div = BigInt(div);
        }

        this.mem.setInt32(addr + 4, Math.floor(Number(value / div)), true);
    }
    storeF32(addr, value) {
        this.mem.setFloat32(addr, value, true);
    }
    storeF64(addr, value) {
        this.mem.setFloat64(addr, value, true);
    }
    storeInt(addr, value) {
        if (this.intSize === 8) {
            this.storeI64(addr, value);
        } else if (this.intSize === 4) {
            this.storeI32(addr, value);
        } else {
            throw new Error("Unhandled `intSize`, expected `4` or `8`");
        }
    }
    storeUint(addr, value) {
        if (this.intSize === 8) {
            this.storeU64(addr, value);
        } else if (this.intSize === 4) {
            this.storeU32(addr, value);
        } else {
            throw new Error("Unhandled `intSize`, expected `4` or `8`");
        }
    }

    // Returned length might not be the same as `value.length` if non-ascii strings are given.
    storeString(addr, value) {
        const src = new TextEncoder().encode(value);
        const dst = new Uint8Array(this.memory.buffer, addr, src.length);
        dst.set(src);
        return src.length;
    }
}

