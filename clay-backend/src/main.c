#include "vendor.c"

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define UNUSED(x) ((void)(x))
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define CSTR_LEN(str) ((sizeof(str) / sizeof(str[0])) - 1)

extern void _os_log(const char *str, int len);
extern int _os_canvas_width(void);
extern int _os_canvas_height(void);
extern void _ui_render(Clay_RenderCommandArray *render_commands);
extern unsigned char __heap_base;

#define os_log(cstr) _os_log(cstr, CSTR_LEN(cstr));

// Clay globals
static Clay_Arena clay_arena = {0};
static uint64_t clay_memory_size = 0;
static void *clay_memory = NULL;
static Clay_RenderCommandArray render_commands = {0};

WASM_EXPORT("os_get_heap_base") void *os_get_heap_base(void) {
  return &__heap_base;
}

WASM_EXPORT("get_render_command_size") int get_render_command_size(void) {
  return sizeof(Clay_RenderCommand);
}

// Error handler for Clay
void HandleClayError(Clay_ErrorData errorData) {
  os_log("Clay Error!");
  UNUSED(errorData);
}

WASM_EXPORT("entrypoint") void entrypoint(void *memory) {
  UNUSED(memory);
  os_log("Entrypoint called!");

  // Calculate required memory for Clay
  clay_memory_size = Clay_MinMemorySize();
  os_log("Clay memory size calculated");

  // Allocate Clay memory from heap
  clay_memory = (void *)(&__heap_base);

  // Initialize Clay arena
  clay_arena =
      Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, clay_memory);
  os_log("Clay arena created");

  // Get canvas dimensions from JavaScript
  int width = _os_canvas_width();
  int height = _os_canvas_height();

  // Initialize Clay
  Clay_Initialize(
      clay_arena,
      (Clay_Dimensions){.width = (float)width, .height = (float)height},
      (Clay_ErrorHandler){.errorHandlerFunction = HandleClayError});
  os_log("Clay initialized!");
}

// Main update and render function called each frame
WASM_EXPORT("update_and_render") void update_and_render(void *memory) {
  UNUSED(memory);

  // Get canvas dimensions from JavaScript
  int width = _os_canvas_width();
  int height = _os_canvas_height();

  // Set layout dimensions (should match canvas size)
  Clay_SetLayoutDimensions(
      (Clay_Dimensions){.width = (float)width, .height = (float)height});

  // Begin layout
  Clay_BeginLayout();

  // Create a simple UI with one red rectangle
  CLAY(CLAY_ID("MainContainer"),
       CLAY__INIT(Clay_ElementDeclaration){
           .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                 .height = CLAY_SIZING_GROW()}}}) {

    CLAY(CLAY_ID("RedRectangle"),
         CLAY__INIT(Clay_ElementDeclaration){
             .layout =
                 {
                     .sizing = {.width = CLAY_SIZING_FIXED(400),
                                .height = CLAY_SIZING_FIXED(300)},
                 },
             .backgroundColor = {255, 100, 100, 255}}) {}
  }

  // End layout and get render commands
  render_commands = Clay_EndLayout();

  // Call JavaScript to render
  _ui_render(&render_commands);
}
