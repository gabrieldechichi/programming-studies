// Include lib .c files directly
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/allocator_pool.c"
#include "lib/string_builder.c"
#include "lib/thread_context.h"
#include "os/os.h"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"

#ifdef WIN32
#include "gpu_backend_d3d11.c"
#endif
#ifdef WASM
#include "gpu_backend_webgpu.c"
#endif
#include "gpu.c"
#include "renderer.c"
#include "mesh.c"
#include "lib/handle.c"
#include "vendor.c"
#include "lib/math.h"
#include "cube.h"
#include "renderer.h"
#include "input.h"
#include "camera.h"
#include "camera.c"
#include "flycam.c"
#include "input.c"
#include "lib/random.c"
#include "context.c"
#include "ecs/ecs_entity.c"
#include "ecs/ecs_table.c"
#include "assets.c"


#ifdef WIN32
#include "os/os_win32.c"
#include "entrypoint_win32.c"
#endif
#ifdef WASM
#include "os/os_wasm.c"
#include "entrypoint.c"
#endif



#include "demos/demo_mesh_loading.c"
// #include "demos/demo_fish.c"
// #include "demos/demo_asset_loading.c"
// #include "demos/demo_cube_instancing.c"
// #include "demos/demo_cube.c"
// #include "demos/demo_triangle.c"
// #include "demos/demo_triangle_texture.c"
// #include "demos/demo_triangle_transform.c"
// #include "demos/demo_triangle_mvp.c"
// #include "demos/demo_triangle_renderer.c"
// #include "demos/demo_triangle_msaa.c"
// #include "demos/demo_triangle_renderer_texture.c"
// #include "demos/demo_renderer.c"
// #include "demos/demo_hello_world.c"
// #include "demos/demo_multicore_tasks.c"
// #include "demos/demo_ecs.c"
// #include "demos/demo_ecs_components.c"
// #include "demos/demo_ecs_tables.c"
// #include "demos/demo_ecs_add_remove.c"
// #include "demos/demo_ecs_query.c"
// #include "demos/demo_ecs_query_cache.c"
// #include "demos/demo_ecs_inout.c"
// #include "demos/demo_ecs_change_detection.c"
// #include "demos/demo_ecs_systems.c"
// #include "demos/demo_ecs_boids.c"
