# IMPORTANT
- On your first response, confirm you've read the instructions and you will follow them. Confirm you will not use the standard library, make builds and fix compile errors.

## What you are working on

This is a C + WebGPU 3d game engine prototype

This project uses a unity build system:
- To add new files for compilation, include the .c file on main.c

## CRITICAL: File Editing on Windows

### ⚠️ MANDATORY: Always Use Backslashes on Windows for File Paths

**When using Edit or MultiEdit tools on Windows, you MUST use backslashes (`\`) in file paths, NOT forward slashes (`/`).**

### IMPORTANT - ALWAYS FOLLOW THIS

- ALWAYS BUILD the project before calling a task done
- ALWAYS FIX compile errors after changes. Here are the build commands for each project
    - **Build and run game**:`make`
- NEVER use C standard library. See`/lib` for any library code you need, or write it yourself
- NEVER use `malloc`/`calloc`/`alloca` - every system in the game has allocators
- NEVER use of printf for logging. Use LOG_INFO, LOG_WARN and LOG_ERROR, with just % as a formatter, and FMT_STR, FMT_UINT wrapping the value.
- USE COMMENTS SPARINGLY, DO NOT COMMENT every line.