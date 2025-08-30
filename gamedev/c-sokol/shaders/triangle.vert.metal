#include <metal_stdlib>
using namespace metal;

struct vs_in {
  float2 pos [[attribute(0)]];
  float4 color [[attribute(1)]];
};

struct vs_out {
  float4 pos [[position]];
  float4 color;
};

vertex vs_out vs_main(vs_in inp [[stage_in]]) {
  vs_out outp;
  outp.pos = float4(inp.pos, 0.0, 1.0);
  outp.color = inp.color;
  return outp;
}