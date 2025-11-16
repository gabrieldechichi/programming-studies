#include "hotreload.h"
#include "os.h"
#include <stdio.h>

internal void hotreload_clear_code(HotReloadAppCode* code) {
    code->is_valid = false;
    code->init = NULL;
    code->update_and_render = NULL;
    code->on_reload = NULL;
    code->lib_handle = NULL;
}

void hotreload_unload_game_code(HotReloadAppCode* code) {
    OsDynLib lib_handle = code->lib_handle;
    hotreload_clear_code(code);

    if (lib_handle) {
        os_dynlib_unload(lib_handle);
    }
}

HotReloadAppCode hotreload_load_game_code(const char* lib_path, const char* temp_path) {
    HotReloadAppCode result = {0};

    os_file_remove(temp_path);

    if (!os_file_copy(lib_path, temp_path)) {
        LOG_ERROR("Failed to copy game library from % to %",
                  FMT_STR(lib_path), FMT_STR(temp_path));
        return result;
    }

    result.last_file_info = os_file_info(lib_path);

    result.lib_handle = os_dynlib_load(temp_path);
    if (!result.lib_handle) {
        LOG_ERROR("Failed to load game library: %", FMT_STR(temp_path));
        return result;
    }

    result.init = (app_init_fn)os_dynlib_get_symbol(result.lib_handle, "app_init");
    result.update_and_render = (app_update_and_render_fn)os_dynlib_get_symbol(result.lib_handle, "app_update_and_render");
    result.on_reload = (app_on_reload_fn)os_dynlib_get_symbol(result.lib_handle, "app_on_reload");

    result.is_valid = (result.init && result.update_and_render && result.on_reload);

    if (!result.is_valid) {
        LOG_ERROR("Failed to find required game functions in library");
        hotreload_unload_game_code(&result);
    }

    return result;
}

b32 hotreload_check_and_reload(HotReloadAppCode* code, AppMemory* memory,
                                const char* lib_path, const char* temp_path, f32 dt) {
    UNUSED(dt);
#if defined(WIN64) || defined(MACOS)
    char lock_file_path[1024];
    snprintf(lock_file_path, sizeof(lock_file_path), "%s.lock", lib_path);
    if (os_file_exists(lock_file_path)) {
        return false;
    }
#endif

    OsFileInfo new_file_info = os_file_info(lib_path);
    if (new_file_info.exists &&
        new_file_info.modification_time != code->last_file_info.modification_time) {

        hotreload_unload_game_code(code);
        *code = hotreload_load_game_code(lib_path, temp_path);

        if (code->is_valid) {
            LOG_INFO("🔥 Hot-reloaded game library");

            if (code->on_reload) {
                code->on_reload(memory);
            }
            return true;
        }
    }

    return false;
}