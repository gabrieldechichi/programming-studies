#include "memory.h"
#include "os.h"
#include "str.h"
#include "typedefs.h"
#include "math.h"
#include <stdarg.h>

#include "assert.h"
#include "str.h"

#include "os.c"
#include "thread.c"
#include "memory.c"
#include "json_parser.c"
#include "msdf_atlas.c"
#include "vendor.c"

extern void _os_log(const char *str, int len);
extern int _os_canvas_width(void);
extern int _os_canvas_height(void);
extern float _os_get_dpr(void);
extern unsigned char __heap_base;

int printf(const char *__restrict, ...);

#define app_log(fmt, ...)                                                      \
  printf("%s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__);

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
extern void _renderer_upload_msdf_atlas(const u8 *image_data, i32 width,
                                        i32 height, i32 channels);
extern void _renderer_draw_msdf_glyph(float x, float y, float width,
                                      float height, float u0, float v0,
                                      float u1, float v1, float r, float g,
                                      float b, float a, float fontSize,
                                      float distanceRange);

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

// MSDF Text Renderer
typedef struct {
  UIFontAsset *asset;  // Single binary asset containing all font data
  b32 initialized;
} TextRenderer;

// App State
typedef struct {
  ArenaAllocator main_arena;
  ThreadContext tctx;
  Clay_Arena clay_arena;
  Clay_RenderCommandArray render_commands;

  OsFileReadOp atlas_json_read_op;
  OsFileReadOp atlas_png_read_op;
  u8 *atlas_json_bytes;
  size_t atlas_json_len;
  u8 *atlas_png_bytes;
  size_t atlas_png_len;

  TextRenderer text_renderer;
} AppState;

// Test image URL (placeholder - replace with actual image)
static const char *test_image_url = "https://pbs.twimg.com/profile_images/"
                                    "1915539238688624640/PpVk5yH7_400x400.png";

WASM_EXPORT("os_get_heap_base") void *os_get_heap_base(void) {
  return &__heap_base;
}

// Error handler for Clay
void HandleClayError(Clay_ErrorData errorData) {
  app_log("Clay Error!");
  UNUSED(errorData);
}

// Helper: Find glyph by unicode in atlas
static MsdfGlyph *find_glyph(UIFontAsset *asset, u32 unicode) {
  MsdfGlyph *glyphs = ui_font_asset_get_glyphs(asset);
  for (u32 i = 0; i < asset->glyph_count; i++) {
    if (glyphs[i].unicode == unicode) {
      return &glyphs[i];
    }
  }
  return NULL;
}

// Clay text measurement function
Clay_Dimensions MeasureText(Clay_StringSlice text,
                            Clay_TextElementConfig *config, void *userData) {
  AppState *app_state = (AppState *)userData;

  debug_assert(app_state);

  if (!app_state->text_renderer.initialized) {
    return (Clay_Dimensions){0, 0};
  }

  UIFontAsset *asset = app_state->text_renderer.asset;
  f32 fontSize = config->fontSize;

  // Calculate width by accumulating glyph advances
  f32 width = 0.0f;
  for (i32 i = 0; i < text.length; i++) {
    u32 codepoint = (u32)text.chars[i];
    MsdfGlyph *glyph = find_glyph(asset, codepoint);

    if (glyph) {
      // advance is in em units, multiply by fontSize for pixels
      width += glyph->advance * fontSize;
    }
  }

  // Calculate height from font metrics (also in em units)
  f32 height = (asset->metrics.ascender - asset->metrics.descender) * fontSize;

  app_log("MeasureText: '%.*s' fontSize=%f -> width=%f height=%f", text.length,
          text.chars, fontSize, width, height);

  return (Clay_Dimensions){width, height};
}

void ui_render(AppState *app_state, Clay_RenderCommandArray *commands) {
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

    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextRenderData *text_data = &cmd->renderData.text;

      if (!app_state->text_renderer.initialized) {
        break;
      }

      UIFontAsset *asset = app_state->text_renderer.asset;
      f32 fontSize = text_data->fontSize;
      f32 distanceRange = asset->atlas.distanceRange;

      // Calculate baseline position
      f32 baseline_y = cmd->boundingBox.y + asset->metrics.ascender * fontSize;
      f32 x = cmd->boundingBox.x;

      // Render each character
      for (i32 i = 0; i < text_data->stringContents.length; i++) {
        u32 codepoint = (u32)text_data->stringContents.chars[i];
        MsdfGlyph *glyph = find_glyph(asset, codepoint);
        assert(glyph);

        if (!glyph || !glyph->has_visual) {
          // Advance for non-visual glyphs (like space)
          if (glyph) {
            x += glyph->advance * fontSize;
          }
          continue;
        }

        // Calculate glyph position using planeBounds (in em units)
        // Canvas has Y increasing downward, so we subtract from baseline to go
        // UP
        f32 glyph_x = x + glyph->planeBounds.left * fontSize;
        f32 glyph_y = baseline_y - glyph->planeBounds.top * fontSize;
        f32 glyph_w =
            (glyph->planeBounds.right - glyph->planeBounds.left) * fontSize;
        f32 glyph_h =
            (glyph->planeBounds.top - glyph->planeBounds.bottom) * fontSize;

        // Calculate UV coordinates from atlasBounds (in pixels)
        f32 u0 = glyph->atlasBounds.left / asset->atlas.width;
        f32 v0 = glyph->atlasBounds.top / asset->atlas.height;
        f32 u1 = glyph->atlasBounds.right / asset->atlas.width;
        f32 v1 = glyph->atlasBounds.bottom / asset->atlas.height;

        // Draw glyph with MSDF
        _renderer_draw_msdf_glyph(
            glyph_x, glyph_y, glyph_w, glyph_h, u0, v0, u1, v1,
            text_data->textColor.r / 255.0f, text_data->textColor.g / 255.0f,
            text_data->textColor.b / 255.0f, text_data->textColor.a / 255.0f,
            fontSize, distanceRange);

        // Advance cursor
        x += glyph->advance * fontSize;
      }

      break;
    }

    case CLAY_RENDER_COMMAND_TYPE_NONE:
      break;

    default:
      break;
    }
  }
}

WASM_EXPORT("entrypoint") void entrypoint(void *memory, u64 memory_size) {
  app_log("entrypoint: memory size %f", BYTES_TO_MB(memory_size));

  AppState *app_state = (AppState *)memory;
  app_state->main_arena = arena_from_buffer((uint8 *)memory + sizeof(AppState),
                                            memory_size - sizeof(AppState));
  ArenaAllocator temp_arena =
      arena_from_buffer(arena_alloc(&app_state->main_arena, MB(64)), MB(64));

  app_state->tctx.temp_allocator = temp_arena;
  tctx_set(&app_state->tctx);
  app_log("ThreadContext initialized");

  // Calculate Clay's memory requirements
  uint64_t clay_memory_size = Clay_MinMemorySize();
  app_log("Clay memory size calculated");

  // Allocate Clay's memory from main arena
  void *clay_memory = arena_alloc(&app_state->main_arena, clay_memory_size);
  assert(clay_memory);

  app_state->clay_arena =
      Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, clay_memory);

  // Get canvas dimensions from JavaScript
  int width = _os_canvas_width();
  int height = _os_canvas_height();

  // Initialize Clay
  Clay_Initialize(
      app_state->clay_arena,
      (Clay_Dimensions){.width = (float)width, .height = (float)height},
      (Clay_ErrorHandler){.errorHandlerFunction = HandleClayError});
  app_log("Clay initialized!");

  // Register text measurement function with Clay
  Clay_SetMeasureTextFunction(MeasureText, app_state);
  app_log("Clay text measurement function registered!");

  app_state->atlas_json_read_op =
      os_start_read_file("Roboto-Regular-atlas.json");
  app_state->atlas_png_read_op = os_start_read_file("Roboto-Regular-atlas.png");
}

Clay_RenderCommandArray test_text() {
  Clay_BeginLayout();

  // Create a UI with scrolling container to test scissor clipping
  CLAY(CLAY_ID("MainContainer"),
       {.layout = {
            .sizing = {.width = CLAY_SIZING_GROW(),
                       .height = CLAY_SIZING_GROW()},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = 0,
            .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
        }}) {
    CLAY_TEXT(
        CLAY_STRING("Hello World!"),
        CLAY_TEXT_CONFIG({.fontSize = 48, .textColor = {255, 255, 255, 255}}));
  }

  return Clay_EndLayout();
}

Clay_RenderCommandArray test_complete_ui() {
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

    // Test text element
    CLAY_TEXT(
        CLAY_STRING("Hello World!"),
        CLAY_TEXT_CONFIG({.fontSize = 48, .textColor = {255, 255, 255, 255}}));
  }

  return Clay_EndLayout();
}

WASM_EXPORT("update_and_render") void update_and_render(void *memory) {
  AppState *app_state = (AppState *)memory;

  // Temporary storage for parsed data (before packing into UIFontAsset)
  static MsdfAtlasData temp_atlas_data = {0};
  static u8 *temp_png_bytes = NULL;
  static size_t temp_png_len = 0;

  // Load atlas JSON
  if (!app_state->atlas_json_bytes) {
    switch (os_check_read_file(app_state->atlas_json_read_op)) {
    case OS_FILE_READ_STATE_NONE:
      break;
    case OS_FILE_READ_STATE_IN_PROGRESS:
      app_log("loading atlas JSON");
      break;
    case OS_FILE_READ_STATE_COMPLETED: {
      size_t json_size = os_get_file_size(app_state->atlas_json_read_op);
      app_log("atlas JSON loaded %f kb", BYTES_TO_KB(json_size));
      PlatformFileData file_data = {0};
      if (os_get_file_data(app_state->atlas_json_read_op, &file_data,
                           &app_state->main_arena)) {
        Clay_ResetMeasureTextCache();
        if (file_data.success) {
          app_state->atlas_json_bytes = file_data.buffer;
          app_state->atlas_json_len = file_data.buffer_len;

          // Null terminate the JSON string
          app_state->atlas_json_bytes[app_state->atlas_json_len] = '\0';

          // Parse atlas JSON into temporary storage
          Allocator allocator = make_arena_allocator(&app_state->main_arena);
          if (!msdf_parse_atlas((const char *)app_state->atlas_json_bytes,
                                &temp_atlas_data, &allocator)) {
            app_log("Failed to parse atlas JSON!");
          } else {
            app_log("Atlas parsed successfully!");
            app_log("  Atlas: %fx%f, distanceRange=%f, size=%f",
                    temp_atlas_data.atlas.width, temp_atlas_data.atlas.height,
                    temp_atlas_data.atlas.distanceRange,
                    temp_atlas_data.atlas.size);
            app_log("  Metrics: emSize=%f, lineHeight=%f, ascender=%f, "
                    "descender=%f",
                    temp_atlas_data.metrics.emSize,
                    temp_atlas_data.metrics.lineHeight,
                    temp_atlas_data.metrics.ascender,
                    temp_atlas_data.metrics.descender);
            app_log("  Glyphs: %d", temp_atlas_data.glyph_count);
          }
        } else {
          app_log("error reading atlas JSON data");
        }
      }
    } break;
    case OS_FILE_READ_STATE_ERROR:
      app_log("atlas JSON read error");
      break;
    }
  }

  // Load atlas PNG
  if (!app_state->atlas_png_bytes) {
    switch (os_check_read_file(app_state->atlas_png_read_op)) {
    case OS_FILE_READ_STATE_NONE:
      break;
    case OS_FILE_READ_STATE_IN_PROGRESS:
      app_log("loading atlas PNG");
      break;
    case OS_FILE_READ_STATE_COMPLETED: {
      size_t png_size = os_get_file_size(app_state->atlas_png_read_op);
      app_log("atlas PNG loaded %f kb", BYTES_TO_KB(png_size));
      PlatformFileData file_data = {0};
      if (os_get_file_data(app_state->atlas_png_read_op, &file_data,
                           &app_state->main_arena)) {
        if (file_data.success) {
          app_state->atlas_png_bytes = file_data.buffer;
          app_state->atlas_png_len = file_data.buffer_len;
          temp_png_bytes = app_state->atlas_png_bytes;
          temp_png_len = app_state->atlas_png_len;
          app_log("PNG loaded successfully!");
        } else {
          app_log("error reading atlas PNG data");
        }
      }
    } break;
    case OS_FILE_READ_STATE_ERROR:
      app_log("atlas PNG read error");
      break;
    }
  }

  // Pack into UIFontAsset when both JSON and PNG are loaded
  if (!app_state->text_renderer.initialized && temp_atlas_data.glyphs &&
      temp_png_bytes) {
    app_log("Packing font asset...");

    // Calculate memory layout
    size_t header_size = sizeof(UIFontAsset);
    size_t glyphs_size = temp_atlas_data.glyph_count * sizeof(MsdfGlyph);
    size_t total_size = header_size + glyphs_size + temp_png_len;

    // Allocate single buffer for entire asset
    UIFontAsset *asset =
        (UIFontAsset *)arena_alloc(&app_state->main_arena, total_size);
    app_log("  Allocated %f kb for font asset", BYTES_TO_KB(total_size));

    // Copy header data
    asset->atlas = temp_atlas_data.atlas;
    asset->metrics = temp_atlas_data.metrics;
    asset->glyph_count = temp_atlas_data.glyph_count;
    asset->glyphs_offset = header_size;
    asset->image_data_offset = header_size + glyphs_size;
    asset->image_data_size = (u32)temp_png_len;

    // Copy glyph array inline
    MsdfGlyph *packed_glyphs = ui_font_asset_get_glyphs(asset);
    for (u32 i = 0; i < temp_atlas_data.glyph_count; i++) {
      packed_glyphs[i] = temp_atlas_data.glyphs[i];
    }

    // Copy PNG data inline
    u8 *packed_image = ui_font_asset_get_image_data(asset);
    for (size_t i = 0; i < temp_png_len; i++) {
      packed_image[i] = temp_png_bytes[i];
    }

    app_log("Font asset packed successfully!");
    app_log("  Header: %zu bytes at offset 0", header_size);
    app_log("  Glyphs: %zu bytes at offset %u", glyphs_size,
            asset->glyphs_offset);
    app_log("  PNG data: %u bytes at offset %u", asset->image_data_size,
            asset->image_data_offset);

    // Decode PNG with stb_image
    stbi_set_flip_vertically_on_load(1);
    i32 width, height, channels;
    u8 *image_data =
        stbi_load_from_memory(packed_image, (i32)asset->image_data_size,
                              &width, &height, &channels, 0);

    if (!image_data) {
      app_log("Failed to parse PNG from packed asset!");
    } else {
      app_log("PNG decoded: %dx%d, channels=%d", width, height, channels);

      // Upload atlas texture to WebGL
      _renderer_upload_msdf_atlas(image_data, width, height, channels);

      // Set asset and mark initialized
      app_state->text_renderer.asset = asset;
      app_state->text_renderer.initialized = true;
      app_log("MSDF atlas uploaded to GPU - renderer initialized!");
    }
  }

  int width = _os_canvas_width();
  int height = _os_canvas_height();

  Clay_SetLayoutDimensions(
      (Clay_Dimensions){.width = (float)width, .height = (float)height});

  app_state->render_commands = test_text();
  ui_render(app_state, &app_state->render_commands);

  arena_reset(&tctx_current()->temp_allocator);
}
