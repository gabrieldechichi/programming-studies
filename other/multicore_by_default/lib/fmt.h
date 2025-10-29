/*
    fmt.h - String formatting (no C stdlib)

    OVERVIEW

    --- Custom string formatting instead of printf/sprintf

    --- use % as placeholder, type-safe args with FMT_* macros

    --- supports: float, int, uint, hex, char, string

    --- used by logging system (LOG_INFO, LOG_ERROR, etc)

    USAGE
        char buffer[256];
        fmt_string(buffer, sizeof(buffer), "x=%, y=%",
            &(FmtArgs){(FmtArg[]){FMT_INT(x), FMT_FLOAT(y)}, 2});

        LOG_INFO("Player at %, %", FMT_FLOAT(pos.x), FMT_FLOAT(pos.y));
*/

#ifndef H_FORMAT
#define H_FORMAT

#include "typedefs.h"

typedef enum {
  FMTARG_FLOAT,
  FMTARG_INT,
  FMTARG_UINT,
  FMTARG_CHAR,
  FMTARG_STR,
  FMTARG_HEX,
} FmtArgType;
#define FMT_FLOAT(v) ((FmtArg){.type = FMTARG_FLOAT, .value_f32 = (v)})
#define FMT_INT(v) ((FmtArg){.type = FMTARG_INT, .value_i32 = (v)})
#define FMT_UINT(v) ((FmtArg){.type = FMTARG_UINT, .value_u32 = (v)})
#define FMT_HEX(v) ((FmtArg){.type = FMTARG_HEX, .value_hex = (v)})
#define FMT_CHAR(v) ((FmtArg){.type = FMTARG_CHAR, .value_char = (v)})
#define FMT_STR(v) ((FmtArg){.type = FMTARG_STR, .value_str = (v)})

typedef struct {
  FmtArgType type;
  union {
    f64 value_f32;
    i64 value_i32;
    u64 value_u32;
    u64 value_hex;
    char value_char;
    const char *value_str;
  };
} FmtArg;

typedef struct {
  FmtArg *args;
  uint8 args_size;
} FmtArgs;

size_t fmt_string(char *buffer, size_t buffer_size, const char *fmt,
                  const FmtArgs *args);

#endif
