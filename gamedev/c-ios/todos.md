- [ ] sokol generic shader tool
    - [ ] add binary for all platforms
    - [ ] C file that compiles everything instead of make file + handle ifdefs?
        - [ ] maybe with job system
- [x] test on windows
- [x] test on iOS?
- [x] C build system
- [ ] Release build
- [x] Vendor separate build
- [ ] cross platform input?
- [ ] move to engine?
- [ ] job system? (maybe too much)


Remaining Optimization Opportunities:

  1. Batch frame processing: Currently processes frames sequentially with blocking waits. Could process 2-3 frames in parallel using multiple
  texture sets.
  2. Direct YUV rendering: Instead of render→compute→readback, could create YUV render targets and render directly in YUV format.
  3. Dynamic command buffer allocation: Allocate command buffers based on actual frame count rather than MAX_FRAMES.
  4. Pipeline caching: Cache created pipelines to disk to speed up subsequent runs.
  5. Memory pooling: Reuse frame data buffers between requests instead of reallocating.


- [ ] direct YUV rendering?
