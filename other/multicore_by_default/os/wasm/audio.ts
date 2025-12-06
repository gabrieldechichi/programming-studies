interface WasmMemory {
  memory: WebAssembly.Memory;
  loadF32Array(ptr: number, count: number): Float32Array;
}

interface Platform {
  audioContext: AudioContext | null;
  audioWorkletNode: AudioWorkletNode | null;
}

export function createAudioFunctions(wasmMemory: WasmMemory, platform: Platform) {
  let samplesNeeded = 0;

  async function initializeAudio(): Promise<number> {
    try {
      const audioContext = new (window.AudioContext ||
        (window as any).webkitAudioContext)();

      await audioContext.audioWorklet.addModule('./audio_worklet.js');

      const audioWorkletNode = new AudioWorkletNode(
        audioContext,
        "game-audio-processor",
      );
      audioWorkletNode.connect(audioContext.destination);

      const resumeAudio = async () => {
        if (audioContext?.state === "suspended") {
          await audioContext.resume();
          console.log("Audio context resumed");
        }
        document.removeEventListener("click", resumeAudio);
        document.removeEventListener("keydown", resumeAudio);
      };

      document.addEventListener("click", resumeAudio);
      document.addEventListener("keydown", resumeAudio);

      console.log(`Audio initialized: ${audioContext.sampleRate}Hz`);

      platform.audioContext = audioContext;
      platform.audioWorkletNode = audioWorkletNode;

      const targetFPS = 60;
      const samplesPerFrame = Math.floor(audioContext.sampleRate / targetFPS);
      samplesNeeded = samplesPerFrame * 2;

      return audioContext.sampleRate;
    } catch (error) {
      console.error("Failed to initialize audio:", error);
      return 48000;
    }
  }

  function _platform_audio_write_samples(samplesPtr: number, sampleCount: number): void {
    const { audioWorkletNode } = platform;
    if (!audioWorkletNode) {
      return;
    }

    const samples = wasmMemory.loadF32Array(samplesPtr, sampleCount);
    const samplesCopy = new Float32Array(samples);
    audioWorkletNode.port.postMessage({
      type: "audio-samples",
      samples: samplesCopy,
    });
  }

  function _platform_audio_get_sample_rate(): number {
    return platform.audioContext?.sampleRate || 48000;
  }

  function _platform_audio_get_samples_needed(): number {
    return samplesNeeded;
  }

  function _platform_audio_update(): void {
  }

  function _platform_audio_shutdown(): void {
    if (platform.audioContext) {
      platform.audioContext.close();
      platform.audioContext = null;
    }
    if (platform.audioWorkletNode) {
      platform.audioWorkletNode.disconnect();
      platform.audioWorkletNode = null;
    }
  }

  return {
    initializeAudio,
    _platform_audio_write_samples,
    _platform_audio_get_sample_rate,
    _platform_audio_get_samples_needed,
    _platform_audio_update,
    _platform_audio_shutdown,
  };
}
