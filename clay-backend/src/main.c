#include "vendor.c"

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define UNUSED(x) ((void)(x))
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define CSTR_LEN(str) ((sizeof(str) / sizeof(str[0])) - 1)

extern void _os_log(const char *str, int len);
extern int _os_canvas_width(void);
extern int _os_canvas_height(void);
extern unsigned char __heap_base;

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
extern void _renderer_scissor_start(float x, float y, float width, float height);
extern void _renderer_scissor_end(void);

#define os_log(cstr) _os_log(cstr, CSTR_LEN(cstr));

// Clay globals
static Clay_Arena clay_arena = {0};
static uint64_t clay_memory_size = 0;
static void *clay_memory = NULL;
static Clay_RenderCommandArray render_commands = {0};

// Test image URL (placeholder - replace with actual image)
static const char *test_image_url = "https://picsum.photos/200";

WASM_EXPORT("os_get_heap_base") void *os_get_heap_base(void) {
  return &__heap_base;
}

// Error handler for Clay
void HandleClayError(Clay_ErrorData errorData) {
  os_log("Clay Error!");
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

WASM_EXPORT("entrypoint") void entrypoint(void *memory) {
  UNUSED(memory);
  os_log("Entrypoint called!");

  clay_memory_size = Clay_MinMemorySize();
  os_log("Clay memory size calculated");

  clay_memory = memory;

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

WASM_EXPORT("update_and_render") void update_and_render(void *memory) {
  UNUSED(memory);

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

  render_commands = Clay_EndLayout();

  // Render using C-side function
  ui_render(&render_commands);
}
