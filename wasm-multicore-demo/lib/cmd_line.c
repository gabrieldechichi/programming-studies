#include "cmd_line.h"
#include "lib/typedefs.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/array.h"
#include "os/os.h"

CmdLineParser cmdline_create(Allocator *allocator) {
  CmdLineParser parser = {0};
  parser.allocator = allocator;
  parser.registered_commands = dyn_arr_new_alloc(allocator, String, 32);
  parser.parsed_commands = dyn_arr_new_alloc(allocator, String, 32);
  parser.flags = dyn_arr_new_alloc(allocator, CmdArg, 32);
  parser.options = dyn_arr_new_alloc(allocator, CmdArg, 32);
  return parser;
}

void cmdline_add_command(CmdLineParser *parser, const char *name) {
  String cmd = str_from_cstr_alloc(name, parser->allocator);
  arr_append(parser->registered_commands, cmd);
}

void cmdline_add_flag(CmdLineParser *parser, const char *name) {
  CmdArg arg = {0};
  arg.name = str_from_cstr_alloc(name, parser->allocator);
  arg.type = CMD_ARG_TYPE_FLAG;
  arg.flag_value = false;
  arg.found = false;
  arr_append(parser->flags, arg);
}

void cmdline_add_option(CmdLineParser *parser, const char *name) {
  CmdArg arg = {0};
  arg.name = str_from_cstr_alloc(name, parser->allocator);
  arg.type = CMD_ARG_TYPE_OPTION;
  arg.option_value = (String){0};
  arg.found = false;
  arr_append(parser->options, arg);
}

b32 cmdline_parse(CmdLineParser *parser, i32 argc, char *argv[]) {
  b32 parsing_commands = true;

  for (i32 i = 1; i < argc; i++) {
    const char* arg_cstr = argv[i];
    u32 arg_len = str_len(arg_cstr);

    if (arg_len > 2 && arg_cstr[0] == '-' && arg_cstr[1] == '-') {
      parsing_commands = false;  // Once we see a flag, no more commands allowed

      const char* flag_name_cstr = arg_cstr + 2;
      u32 flag_name_len = arg_len - 2;

      b32 found_flag = false;
      for (u32 j = 0; j < parser->flags.len; j++) {
        if (str_equal_len(parser->flags.items[j].name.value, parser->flags.items[j].name.len,
                         flag_name_cstr, flag_name_len)) {
          parser->flags.items[j].flag_value = true;
          parser->flags.items[j].found = true;
          found_flag = true;
          break;
        }
      }

      if (!found_flag) {
        for (u32 j = 0; j < parser->options.len; j++) {
          if (str_equal_len(parser->options.items[j].name.value, parser->options.items[j].name.len,
                           flag_name_cstr, flag_name_len)) {
            if (i + 1 < argc) {
              i++;
              const char* val_cstr = argv[i];
              parser->options.items[j].option_value = str_from_cstr_alloc(val_cstr, parser->allocator);
              parser->options.items[j].found = true;
              found_flag = true;
            } else {
              LOG_ERROR("Option --% requires a value", FMT_STR(flag_name_cstr));
              return false;
            }
            break;
          }
        }
      }

      if (!found_flag) {
        LOG_ERROR("Unknown flag/option: %", FMT_STR(arg_cstr));
        return false;
      }
    } else {
      if (!parsing_commands) {
        LOG_ERROR("Commands must come before flags. Found '%' after flags.", FMT_STR(arg_cstr));
        return false;
      }

      b32 valid_command = false;
      for (u32 j = 0; j < parser->registered_commands.len; j++) {
        if (str_equal_len(parser->registered_commands.items[j].value,
                         parser->registered_commands.items[j].len,
                         arg_cstr, arg_len)) {
          valid_command = true;
          break;
        }
      }

      if (!valid_command && parser->registered_commands.len > 0) {
        LOG_ERROR("Unknown command '%'. Valid commands are:", FMT_STR(arg_cstr));
        for (u32 j = 0; j < parser->registered_commands.len; j++) {
          LOG_INFO("  %", FMT_STR_VIEW(parser->registered_commands.items[j]));
        }
        return false;
      }

      String cmd = str_from_cstr_with_len_alloc(arg_cstr, arg_len, parser->allocator);
      arr_append(parser->parsed_commands, cmd);
    }
  }

  return true;
}

b32 cmdline_has_command(CmdLineParser *parser, const char *command) {
  u32 cmd_len = str_len(command);
  for (u32 i = 0; i < parser->parsed_commands.len; i++) {
    if (str_equal_len(parser->parsed_commands.items[i].value, parser->parsed_commands.items[i].len,
                     command, cmd_len)) {
      return true;
    }
  }
  return false;
}

b32 cmdline_has_flag(CmdLineParser *parser, const char *flag) {
  u32 flag_len = str_len(flag);
  for (u32 i = 0; i < parser->flags.len; i++) {
    if (str_equal_len(parser->flags.items[i].name.value, parser->flags.items[i].name.len,
                     flag, flag_len)) {
      return parser->flags.items[i].found && parser->flags.items[i].flag_value;
    }
  }
  return false;
}

String cmdline_get_option(CmdLineParser *parser, const char *option) {
  u32 option_len = str_len(option);
  for (u32 i = 0; i < parser->options.len; i++) {
    if (str_equal_len(parser->options.items[i].name.value, parser->options.items[i].name.len,
                     option, option_len)) {
      return parser->options.items[i].option_value;
    }
  }
  return (String){0};
}

String cmdline_get_command_at(CmdLineParser *parser, u32 index) {
  if (index < parser->parsed_commands.len) {
    return parser->parsed_commands.items[index];
  }
  return (String){0};
}
