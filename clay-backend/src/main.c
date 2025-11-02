// Clay WebGL2 Backend - WASM Entry Point
// Freestanding environment (no stdlib)

// External JS function we can call for logging
extern void js_log(int value);

// Export the entrypoint function to JavaScript
__attribute__((export_name("entrypoint")))
int entrypoint(void) {
    // Return a test value to verify WASM is working
    // Later this will initialize Clay and setup rendering
    js_log(42);
    return 1337;
}

