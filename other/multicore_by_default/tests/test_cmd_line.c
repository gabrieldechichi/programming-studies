#include "../lib/cmd_line.h"
#include "../lib/test.h"
#include "../lib/memory.h"
#include "../lib/string.h"

void test_cmdline_create(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  assert_eq(parser.registered_commands.len, 0);
  assert_eq(parser.parsed_commands.len, 0);
  assert_eq(parser.flags.len, 0);
  assert_eq(parser.options.len, 0);
}

void test_cmdline_add_command(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "run");
  cmdline_add_command(&parser, "test");

  assert_eq(parser.registered_commands.len, 3);
  assert_true(str_equal(parser.registered_commands.items[0].value, "build"));
  assert_true(str_equal(parser.registered_commands.items[1].value, "run"));
  assert_true(str_equal(parser.registered_commands.items[2].value, "test"));
}

void test_cmdline_add_flag(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_flag(&parser, "verbose");
  cmdline_add_flag(&parser, "debug");

  assert_eq(parser.flags.len, 2);
  assert_true(str_equal(parser.flags.items[0].name.value, "verbose"));
  assert_true(str_equal(parser.flags.items[1].name.value, "debug"));
  assert_eq(parser.flags.items[0].type, CMD_ARG_TYPE_FLAG);
  assert_false(parser.flags.items[0].found);
  assert_false(parser.flags.items[0].flag_value);
}

void test_cmdline_add_option(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_option(&parser, "output");
  cmdline_add_option(&parser, "config");

  assert_eq(parser.options.len, 2);
  assert_true(str_equal(parser.options.items[0].name.value, "output"));
  assert_true(str_equal(parser.options.items[1].name.value, "config"));
  assert_eq(parser.options.items[0].type, CMD_ARG_TYPE_OPTION);
  assert_false(parser.options.items[0].found);
}

void test_cmdline_parse_commands_only(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "test");
  cmdline_add_command(&parser, "run");

  char* argv[] = {"program", "build", "test"};
  b32 result = cmdline_parse(&parser, 3, argv);

  assert_true(result);
  assert_eq(parser.parsed_commands.len, 2);
  assert_true(str_equal(parser.parsed_commands.items[0].value, "build"));
  assert_true(str_equal(parser.parsed_commands.items[1].value, "test"));
}

void test_cmdline_parse_invalid_command(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "test");

  char* argv[] = {"program", "invalid"};
  b32 result = cmdline_parse(&parser, 2, argv);

  assert_false(result);
  assert_eq(parser.parsed_commands.len, 0);
}

void test_cmdline_parse_flags(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_flag(&parser, "verbose");
  cmdline_add_flag(&parser, "debug");

  char* argv[] = {"program", "build", "--verbose", "--debug"};
  b32 result = cmdline_parse(&parser, 4, argv);

  assert_true(result);
  assert_eq(parser.parsed_commands.len, 1);
  assert_true(cmdline_has_flag(&parser, "verbose"));
  assert_true(cmdline_has_flag(&parser, "debug"));
}

void test_cmdline_parse_options(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_option(&parser, "output");
  cmdline_add_option(&parser, "config");

  char* argv[] = {"program", "build", "--output", "/tmp/out", "--config", "debug.cfg"};
  b32 result = cmdline_parse(&parser, 6, argv);

  assert_true(result);
  assert_eq(parser.parsed_commands.len, 1);

  String output = cmdline_get_option(&parser, "output");
  assert_true(str_equal(output.value, "/tmp/out"));

  String config = cmdline_get_option(&parser, "config");
  assert_true(str_equal(config.value, "debug.cfg"));
}

void test_cmdline_parse_mixed(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "macos");
  cmdline_add_command(&parser, "run");
  cmdline_add_flag(&parser, "hotreload");
  cmdline_add_flag(&parser, "verbose");
  cmdline_add_option(&parser, "cfg");

  char* argv[] = {"program", "macos", "run", "--hotreload", "--cfg", "release", "--verbose"};
  b32 result = cmdline_parse(&parser, 7, argv);

  assert_true(result);
  assert_eq(parser.parsed_commands.len, 2);
  assert_true(cmdline_has_command(&parser, "macos"));
  assert_true(cmdline_has_command(&parser, "run"));
  assert_true(cmdline_has_flag(&parser, "hotreload"));
  assert_true(cmdline_has_flag(&parser, "verbose"));

  String cfg = cmdline_get_option(&parser, "cfg");
  assert_true(str_equal(cfg.value, "release"));
}

void test_cmdline_commands_after_flags_error(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "run");
  cmdline_add_flag(&parser, "verbose");

  char* argv[] = {"program", "--verbose", "build"};
  b32 result = cmdline_parse(&parser, 3, argv);

  assert_false(result);
}

void test_cmdline_unknown_flag_error(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_flag(&parser, "verbose");

  char* argv[] = {"program", "build", "--unknown"};
  b32 result = cmdline_parse(&parser, 3, argv);

  assert_false(result);
}

void test_cmdline_option_without_value_error(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_option(&parser, "output");

  char* argv[] = {"program", "build", "--output"};
  b32 result = cmdline_parse(&parser, 3, argv);

  assert_false(result);
}

void test_cmdline_has_command(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "test");

  char* argv[] = {"program", "build", "test"};
  cmdline_parse(&parser, 3, argv);

  assert_true(cmdline_has_command(&parser, "build"));
  assert_true(cmdline_has_command(&parser, "test"));
  assert_false(cmdline_has_command(&parser, "run"));
}

void test_cmdline_get_command_at(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "macos");
  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "run");

  char* argv[] = {"program", "macos", "build"};
  cmdline_parse(&parser, 3, argv);

  String first = cmdline_get_command_at(&parser, 0);
  assert_true(str_equal(first.value, "macos"));

  String second = cmdline_get_command_at(&parser, 1);
  assert_true(str_equal(second.value, "build"));

  String third = cmdline_get_command_at(&parser, 2);
  assert_eq(third.len, 0);  // Out of bounds
}

void test_cmdline_empty_args(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "build");

  char* argv[] = {"program"};
  b32 result = cmdline_parse(&parser, 1, argv);

  assert_true(result);
  assert_eq(parser.parsed_commands.len, 0);
}

void test_cmdline_no_registered_commands(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_flag(&parser, "verbose");

  char* argv[] = {"program", "anything", "--verbose"};
  b32 result = cmdline_parse(&parser, 3, argv);

  assert_true(result);  // Should work if no commands are registered
  assert_eq(parser.parsed_commands.len, 1);
  assert_true(cmdline_has_flag(&parser, "verbose"));
}

void test_cmdline_flag_not_found(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_flag(&parser, "verbose");
  cmdline_add_flag(&parser, "debug");

  char* argv[] = {"program", "--verbose"};
  cmdline_parse(&parser, 2, argv);

  assert_true(cmdline_has_flag(&parser, "verbose"));
  assert_false(cmdline_has_flag(&parser, "debug"));  // Not provided
}

void test_cmdline_option_not_found(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_option(&parser, "output");

  char* argv[] = {"program"};
  cmdline_parse(&parser, 1, argv);

  String output = cmdline_get_option(&parser, "output");
  assert_eq(output.len, 0);  // Not provided, should return empty string
}

void test_cmdline_duplicate_flags(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_flag(&parser, "verbose");

  char* argv[] = {"program", "--verbose", "--verbose"};
  b32 result = cmdline_parse(&parser, 3, argv);

  assert_true(result);
  assert_true(cmdline_has_flag(&parser, "verbose"));
}

void test_cmdline_complex_scenario(TestContext* ctx) {
  CmdLineParser parser = cmdline_create(&ctx->allocator);

  cmdline_add_command(&parser, "linux");
  cmdline_add_command(&parser, "windows");
  cmdline_add_command(&parser, "macos");
  cmdline_add_command(&parser, "build");
  cmdline_add_command(&parser, "test");
  cmdline_add_command(&parser, "clean");

  cmdline_add_flag(&parser, "verbose");
  cmdline_add_flag(&parser, "debug");
  cmdline_add_flag(&parser, "release");

  cmdline_add_option(&parser, "output");
  cmdline_add_option(&parser, "jobs");
  cmdline_add_option(&parser, "target");

  char* argv[] = {"program", "macos", "build", "--debug", "--output", "bin/", "--jobs", "4", "--verbose"};
  b32 result = cmdline_parse(&parser, 9, argv);

  assert_true(result);

  assert_eq(parser.parsed_commands.len, 2);
  assert_true(cmdline_has_command(&parser, "macos"));
  assert_true(cmdline_has_command(&parser, "build"));

  assert_true(cmdline_has_flag(&parser, "debug"));
  assert_true(cmdline_has_flag(&parser, "verbose"));
  assert_false(cmdline_has_flag(&parser, "release"));

  String output = cmdline_get_option(&parser, "output");
  assert_true(str_equal(output.value, "bin/"));

  String jobs = cmdline_get_option(&parser, "jobs");
  assert_true(str_equal(jobs.value, "4"));

  String target = cmdline_get_option(&parser, "target");
  assert_eq(target.len, 0);  // Not provided
}