#ifndef H_CMDLINE
#define H_CMDLINE

#include "lib/string.h"

typedef enum {
  CMD_ARG_TYPE_COMMAND,
  CMD_ARG_TYPE_FLAG,
  CMD_ARG_TYPE_OPTION,
} CmdArgType;

typedef struct {
  String name;
  CmdArgType type;
  union {
    b32 flag_value;
    String option_value;
  };
  b32 found;
} CmdArg;
arr_define(CmdArg);

typedef struct {
  String_DynArray registered_commands;  // Valid command names
  String_DynArray parsed_commands;      // Actual commands from argv
  CmdArg_DynArray flags;
  CmdArg_DynArray options;
  Allocator *allocator;
} CmdLineParser;
#endif
