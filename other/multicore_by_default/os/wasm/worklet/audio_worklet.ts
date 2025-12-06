class AudioStreamBuffer {
  buffer: Float32Array | null;
  capacity: number;
  writePos: number;
  readPos: number;

  constructor() {
    this.buffer = null;
    this.capacity = 1024;
    this.writePos = 0;
    this.readPos = 0;
  }

  get len(): number {
    return (this.writePos - this.readPos + this.capacity) % this.capacity;
  }

  get availableSpace(): number {
    return this.capacity - this.len;
  }

  writeSamples(samples: Float32Array): void {
    const samplesToWrite = samples.length;
    const availableSpace = this.availableSpace;

    if (samplesToWrite > availableSpace) {
      const byteCountToOverride = samplesToWrite - availableSpace;
      console.warn(
        `[AudioWorklet] Buffer overflow, overriding ${byteCountToOverride} samples`,
      );
    }

    for (let i = 0; i < samplesToWrite; i++) {
      this.buffer![this.writePos] = samples[i];
      this.writePos = (this.writePos + 1) % this.capacity;
    }
  }

  readSamples(count: number): Float32Array {
    const availableSampleCount = this.len;
    const samplesToRead = Math.min(count, availableSampleCount);
    const output = new Float32Array(samplesToRead);

    for (let i = 0; i < samplesToRead; i++) {
      output[i] = this.buffer![this.readPos];
      this.readPos = (this.readPos + 1) % this.capacity;
    }

    return output;
  }

  readSample(): number {
    const availableSampleCount = this.len;
    if (availableSampleCount == 0) {
      return 0;
    }

    const sample = this.buffer![this.readPos];
    this.readPos = (this.readPos + 1) % this.capacity;
    return sample;
  }
}

interface AudioMessage {
  type: string;
  samples: Float32Array;
}

class GameAudioProcessor extends AudioWorkletProcessor {
  audioBuffer: AudioStreamBuffer;

  constructor() {
    super();

    const audioBuffer = new AudioStreamBuffer();
    audioBuffer.capacity = 48000 * 16 * 4;
    audioBuffer.writePos = 0;
    audioBuffer.buffer = new Float32Array(audioBuffer.capacity);
    this.audioBuffer = audioBuffer;

    this.port.onmessage = (event: MessageEvent<AudioMessage>) => {
      if (event.data.type === "audio-samples") {
        this.audioBuffer.writeSamples(event.data.samples);
      }
    };

    console.log("[AudioWorklet] GameAudioProcessor initialized");
  }

  process(
    _inputs: Float32Array[][],
    outputs: Float32Array[][],
    _parameters: Record<string, Float32Array>
  ): boolean {
    const output = outputs[0];
    const outputLength = output[0].length;

    const channelCount = output.length;
    if (channelCount == 1) {
      for (let i = 0; i < outputLength; i++) {
        output[0][i] = this.audioBuffer.readSample();
        this.audioBuffer.readSample();
      }
    } else if (channelCount == 2) {
      for (let i = 0; i < outputLength; i++) {
        output[0][i] = this.audioBuffer.readSample();
        output[1][i] = this.audioBuffer.readSample();
      }
    }

    return true;
  }
}

registerProcessor("game-audio-processor", GameAudioProcessor);
