#ifndef H_HOTRELOAD
#define H_HOTRELOAD

#include "context.h"
#include "lib/common.h"
#include "app/app.h"
#include "os.h"

typedef void (*app_init_fn)(AppMemory *);
typedef void (*app_update_and_render_fn)(AppMemory *);
typedef void (*app_on_reload_fn)(AppMemory *);
typedef AppContext *(*game_get_ctx_fn)(AppMemory *);

typedef struct {
  OsDynLib lib_handle;
  OsFileInfo last_file_info;

  app_init_fn init;
  app_update_and_render_fn update_and_render;
  app_on_reload_fn on_reload;

  b32 is_valid;
} HotReloadAppCode;

HotReloadAppCode hotreload_load_game_code(const char *lib_path,
                                           const char *temp_path);
void hotreload_unload_game_code(HotReloadAppCode *code);
b32 hotreload_check_and_reload(HotReloadAppCode *code, AppMemory *memory,
                               const char *lib_path, const char *temp_path,
                               f32 dt);

#endif