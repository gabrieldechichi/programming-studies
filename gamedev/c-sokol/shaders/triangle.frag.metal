#include <metal_stdlib>
using namespace metal;

fragment float4 fs_main(float4 color [[stage_in]]) {
  return color;
}