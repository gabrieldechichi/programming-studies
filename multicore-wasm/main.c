#include "os/os_wasm.c"

WASM_EXPORT(thread_func)
void thread_func(void* arg) {
    (void)arg;
    print("Hello from thread!");
}

WASM_EXPORT(wasm_main)
int wasm_main() {
    print("Before thread launch");

    Thread t = os_thread_launch(thread_func, (void*)0);

    print("After thread launch, before join");

    os_thread_join(t, 0);

    print("After thread join");

    return 0;
}
