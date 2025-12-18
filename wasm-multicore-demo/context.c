#include "context.h"
#include "lib/typedefs.h"

global AppContext *__app_ctx = 0;
AppContext *app_ctx_current() {
  debug_assert_msg(__app_ctx,
                   "App Context not set, please call app_ctx_set on app_init");
  return __app_ctx;
}
void app_ctx_set(AppContext *ctx) { __app_ctx = ctx; }
