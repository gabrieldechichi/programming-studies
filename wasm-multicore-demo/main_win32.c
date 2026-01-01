// Unity build for Windows platform

// Lib .c files
#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/allocator_pool.c"
#include "lib/string_builder.c"
#include "lib/thread_context.h"
#include "os/os.h"
#include "os/os_win32.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/multicore_runtime.c"
#include "lib/handle.c"
#include "lib/math.h"
#include "lib/random.c"

// Input
#include "input.h"
#include "input.c"

// Context
#include "context.c"

// Windows entrypoint
#include "entrypoint_win32.c"

// Demo
#include "demos/demo_hello_world.c"
