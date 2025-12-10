// WASM Threading Test Suite
// Uncomment ONE demo at a time to test each threading feature

#include "lib/typedefs.h"
#include "lib/string.c"
#include "lib/common.c"
#include "lib/thread.c"
#include "os/os_wasm.c"

// === SELECT ONE DEMO ===
// #include "demos/demo_thread_create.c" // 1. Basic thread create/join
// #include "demos/demo_tls.c"          // 2. Thread Local Storage
// #include "demos/demo_shared_memory.c" // 3. Shared memory + race conditions
// #include "demos/demo_mutex.c"        // 4. Mutex lock/unlock
// #include "demos/demo_barrier.c"      // 5. Barrier synchronization
// #include "demos/demo_condvar.c"      // 6. Condition variables
// #include "demos/demo_atomics.c"      // 7. Atomic operations
// #include "demos/demo_semaphore.c"    // 8. Counting semaphore
// #include "demos/demo_rwlock.c"       // 9. Read-write lock
#include "demos/demo_detach.c"       // 10. Thread detach

WASM_EXPORT(wasm_main)
int wasm_main(void) { return demo_main(); }
