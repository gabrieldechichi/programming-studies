#include "vendor.c"

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define UNUSED(x) ((void)(x))
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define CSTR_LEN(str) ((sizeof(str) / sizeof(str[0])) - 1)

extern void _os_log(const char *str, int len);
extern unsigned char __heap_base;

#define os_log(cstr) _os_log(cstr, CSTR_LEN(cstr));

WASM_EXPORT("os_get_heap_base") void *os_get_heap_base(void) {
  return &__heap_base;
}

WASM_EXPORT("entrypoint") void entrypoint(void) {
  os_log("Entrypoint called!");
}

// Main update and render function called each frame
WASM_EXPORT("update_and_render") void update_and_render(void* memory) {
  UNUSED(memory);
  os_log("Update frame");
}
