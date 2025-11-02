// Clay WebGL2 Backend - WASM Entry Point
// Freestanding environment (no stdlib)

// External JS function we can call for logging
extern void os_log(const char *str, int len);

// Heap base provided by the linker
extern unsigned char __heap_base;

// Export heap base getter
__attribute__((export_name("os_get_heap_base")))
void *os_get_heap_base(void) {
    return &__heap_base;
}

// Export the entrypoint function to JavaScript
__attribute__((export_name("entrypoint")))
int entrypoint(void) {
    // Return a test value to verify WASM is working
    // Later this will initialize Clay and setup rendering
    const char *msg = "Entrypoint called!";
    int len = 19; // length of message
    os_log(msg, len);
    return 1337;
}

// Main update and render function called each frame
__attribute__((export_name("update_and_render")))
void update_and_render(float delta_time) {
    // For now, just log to verify the render loop is working
    const char *msg = "Update frame";
    int len = 12; // length of message
    os_log(msg, len);
}

