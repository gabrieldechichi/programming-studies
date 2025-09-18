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
    float32 value_f32;
    int32 value_i32;
    uint32 value_u32;
    uint32 value_hex;
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
