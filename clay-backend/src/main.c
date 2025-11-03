#include "memory.h"
#include "str.h"
#include "typedefs.h"
#include <stdarg.h>

#include "assert.h"
#include "str.h"
#include "thread.c"
#include "memory.c"
#include "vendor.c"

extern void _os_log(const char *str, int len);
extern int _os_canvas_width(void);
extern int _os_canvas_height(void);
extern unsigned char __heap_base;

int printf(const char *__restrict, ...);

// Renderer API - simple functions called from C to JS
extern void _renderer_clear(float r, float g, float b, float a);
extern void _renderer_draw_rect(float x, float y, float width, float height,
                                float r, float g, float b, float a,
                                float corner_top_left, float corner_top_right,
                                float corner_bottom_left,
                                float corner_bottom_right);
extern void _renderer_draw_border(float x, float y, float width, float height,
                                  float r, float g, float b, float a,
                                  float corner_top_left, float corner_top_right,
                                  float corner_bottom_left,
                                  float corner_bottom_right, float border_left,
                                  float border_right, float border_top,
                                  float border_bottom);
extern void _renderer_draw_image(float x, float y, float width, float height,
                                 const char *image_data_ptr, float tint_r,
                                 float tint_g, float tint_b, float tint_a,
                                 float corner_tl, float corner_tr,
                                 float corner_bl, float corner_br);
extern void _renderer_scissor_start(float x, float y, float width,
                                    float height);
extern void _renderer_scissor_end(void);

int printf(const char *format, ...) {
  char buffer[KB(4)];

  int buf_pos = 0;
  int buf_size = KB(4);

  va_list args;
  va_start(args, format);

  for (const char *p = format; *p != '\0' && buf_pos < buf_size - 1; p++) {
    if (*p == '%' && *(p + 1) != '\0') {
      p++; // Skip '%'

      switch (*p) {
      case 'd': { // Integer
        i32 val = va_arg(args, i32);
        char temp[32];
        int len = i32_to_str(val, temp);
        for (int i = 0; i < len && buf_pos < buf_size - 1; i++) {
          buffer[buf_pos++] = temp[i];
        }
        break;
      }

      case 'f': { // Float
        f64 val = va_arg(args, f64);
        char temp[64];
        int len = f32_to_str((f32)val, temp, 2);
        for (int i = 0; i < len && buf_pos < buf_size - 1; i++) {
          buffer[buf_pos++] = temp[i];
        }
        break;
      }

      case 'c': { // Character
        char val = (char)va_arg(args, int);
        buffer[buf_pos++] = val;
        break;
      }

      case 's': { // String
        const char *str = va_arg(args, const char *);
        if (str) {
          while (*str && buf_pos < buf_size - 1) {
            buffer[buf_pos++] = *str++;
          }
        }
        break;
      }

      case '%': { // Literal %
        buffer[buf_pos++] = '%';
        break;
      }

      default:
        buffer[buf_pos++] = '%';
        buffer[buf_pos++] = *p;
        break;
      }
    } else {
      buffer[buf_pos++] = *p;
    }
  }

  va_end(args);

  // Null terminate and log
  buffer[buf_pos] = '\0';
  _os_log(buffer, buf_pos);

  return buf_pos;
}

// App State
typedef struct {
  ArenaAllocator main_arena;
  ThreadContext tctx;
  Clay_Arena clay_arena;
  Clay_RenderCommandArray render_commands;
} AppState;

// Test image URL (placeholder - replace with actual image)
static const char *test_image_url = "https://pbs.twimg.com/profile_images/"
                                    "1915539238688624640/PpVk5yH7_400x400.png";

WASM_EXPORT("os_get_heap_base") void *os_get_heap_base(void) {
  return &__heap_base;
}

// Error handler for Clay
void HandleClayError(Clay_ErrorData errorData) {
  printf("Clay Error!");
  UNUSED(errorData);
}

// Render function - iterates through Clay commands and calls JS renderer
void ui_render(Clay_RenderCommandArray *commands) {
  // Clear screen
  _renderer_clear(0.0f, 0.0f, 0.0f, 1.0f);

  // Iterate through all render commands
  for (int i = 0; i < commands->length; i++) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(commands, i);

    switch (cmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleRenderData *rect = &cmd->renderData.rectangle;
      _renderer_draw_rect(
          cmd->boundingBox.x, cmd->boundingBox.y, cmd->boundingBox.width,
          cmd->boundingBox.height, rect->backgroundColor.r,
          rect->backgroundColor.g, rect->backgroundColor.b,
          rect->backgroundColor.a, rect->cornerRadius.topLeft,
          rect->cornerRadius.topRight, rect->cornerRadius.bottomLeft,
          rect->cornerRadius.bottomRight);
      break;
    }

    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      Clay_BorderRenderData *border = &cmd->renderData.border;
      _renderer_draw_border(
          cmd->boundingBox.x, cmd->boundingBox.y, cmd->boundingBox.width,
          cmd->boundingBox.height, border->color.r, border->color.g,
          border->color.b, border->color.a, border->cornerRadius.topLeft,
          border->cornerRadius.topRight, border->cornerRadius.bottomLeft,
          border->cornerRadius.bottomRight, border->width.left,
          border->width.right, border->width.top, border->width.bottom);
      break;
    }

    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      // Note: Clay_ClipRenderData contains .horizontal and .vertical flags
      // But for scissor test, we always clip both axes (scissor is a rectangle)
      // The flags are informational about which scroll axes are enabled
      _renderer_scissor_start(cmd->boundingBox.x, cmd->boundingBox.y,
                              cmd->boundingBox.width, cmd->boundingBox.height);
      break;
    }

    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      _renderer_scissor_end();
      break;

    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      Clay_ImageRenderData *image = &cmd->renderData.image;
      // imageData is a void* - we interpret it as a char* (URL string)
      _renderer_draw_image(
          cmd->boundingBox.x, cmd->boundingBox.y, cmd->boundingBox.width,
          cmd->boundingBox.height, (const char *)image->imageData,
          image->backgroundColor.r, image->backgroundColor.g,
          image->backgroundColor.b, image->backgroundColor.a,
          image->cornerRadius.topLeft, image->cornerRadius.topRight,
          image->cornerRadius.bottomLeft, image->cornerRadius.bottomRight);
      break;
    }

    case CLAY_RENDER_COMMAND_TYPE_NONE:
      break;

      // TODO: Add other command types (TEXT, CUSTOM)

    default:
      break;
    }
  }
}

WASM_EXPORT("entrypoint") void entrypoint(void *memory, u64 memory_size) {
  printf("entrypoint: memory size %f", BYTES_TO_MB(memory_size));

  AppState *app_state = (AppState *)memory;
  app_state->main_arena = arena_from_buffer((uint8 *)memory + sizeof(AppState),
                                            memory_size - sizeof(AppState));
  ArenaAllocator temp_arena =
      arena_from_buffer(arena_alloc(&app_state->main_arena, MB(64)), MB(64));

  app_state->tctx.temp_allocator = temp_arena;
  tctx_set(&app_state->tctx);
  printf("ThreadContext initialized");

  // Calculate Clay's memory requirements
  uint64_t clay_memory_size = Clay_MinMemorySize();
  printf("Clay memory size calculated");

  // Allocate Clay's memory from main arena
  void *clay_memory = arena_alloc(&app_state->main_arena, clay_memory_size);
  if (!clay_memory) {
    printf("ERROR: Failed to allocate Clay memory!");
    return;
  }

  app_state->clay_arena =
      Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, clay_memory);
  printf("Clay arena created");

  // Get canvas dimensions from JavaScript
  int width = _os_canvas_width();
  int height = _os_canvas_height();

  // Initialize Clay
  Clay_Initialize(
      app_state->clay_arena,
      (Clay_Dimensions){.width = (float)width, .height = (float)height},
      (Clay_ErrorHandler){.errorHandlerFunction = HandleClayError});
  printf("Clay initialized!");
}

WASM_EXPORT("update_and_render") void update_and_render(void *memory) {
  AppState *app_state = (AppState *)memory;

  int width = _os_canvas_width();
  int height = _os_canvas_height();

  Clay_SetLayoutDimensions(
      (Clay_Dimensions){.width = (float)width, .height = (float)height});

  Clay_BeginLayout();

  // Create a UI with scrolling container to test scissor clipping
  CLAY(CLAY_ID("MainContainer"),
       CLAY__INIT(Clay_ElementDeclaration){
           .layout =
               {
                   .sizing =
                       {
                           .width = CLAY_SIZING_GROW(),
                           .height = CLAY_SIZING_GROW(),
                       },
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = 20,
                   .childAlignment =
                       {
                           .x = CLAY_ALIGN_X_CENTER,
                           .y = CLAY_ALIGN_Y_CENTER,
                       },
               },
       }) {

    // Scrolling container with clipping enabled
    CLAY(CLAY_ID("ScrollContainer"),
         CLAY__INIT(Clay_ElementDeclaration){
             .layout =
                 {
                     .sizing = {.width = CLAY_SIZING_FIXED(300),
                                .height = CLAY_SIZING_FIXED(300)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 16,
                 },
             .backgroundColor = {50, 50, 50, 255},
             .cornerRadius = CLAY_CORNER_RADIUS(10),
             .border = {.width = CLAY_BORDER_OUTSIDE(2),
                        .color = {100, 100, 100, 255}},
             .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
         }) {

      // Multiple rectangles to demonstrate scrolling
      CLAY(CLAY_ID("RedRectangle"),
           CLAY__INIT(Clay_ElementDeclaration){
               .layout =
                   {
                       .sizing = {.width = CLAY_SIZING_FIXED(250),
                                  .height = CLAY_SIZING_FIXED(150)},
                   },
               .backgroundColor = {255, 0, 0, 255},
               .cornerRadius = CLAY_CORNER_RADIUS(10),
           }) {}

      CLAY(CLAY_ID("GreenRectangle"),
           CLAY__INIT(Clay_ElementDeclaration){
               .layout =
                   {
                       .sizing = {.width = CLAY_SIZING_FIXED(250),
                                  .height = CLAY_SIZING_FIXED(150)},
                   },
               .backgroundColor = {0, 255, 0, 255},
               .cornerRadius = CLAY_CORNER_RADIUS(10),
           }) {}

      CLAY(CLAY_ID("BlueRectangle"),
           CLAY__INIT(Clay_ElementDeclaration){
               .layout =
                   {
                       .sizing = {.width = CLAY_SIZING_FIXED(250),
                                  .height = CLAY_SIZING_FIXED(150)},
                   },
               .backgroundColor = {0, 100, 255, 255},
               .cornerRadius = CLAY_CORNER_RADIUS(10),
           }) {}
    }

    // Test image with rounded corners (outside clipped container)
    CLAY(CLAY_ID("TestImage"),
         CLAY__INIT(Clay_ElementDeclaration){
             .layout =
                 {
                     .sizing = {.width = CLAY_SIZING_FIXED(200),
                                .height = CLAY_SIZING_FIXED(200)},
                 },
             .image = {.imageData = (void *)test_image_url},
             .cornerRadius = CLAY_CORNER_RADIUS(20),
         }) {}
  }

  app_state->render_commands = Clay_EndLayout();

  // Render using C-side function
  ui_render(&app_state->render_commands);
}
