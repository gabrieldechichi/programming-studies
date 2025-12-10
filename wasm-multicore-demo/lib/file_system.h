/*
    file_system.h - File path utilities

    OVERVIEW

    --- Parse file paths into components (directory, filename, basename, extension)

    USAGE
        FsPathInfo info = fs_path_parse("assets/models/player.fbx");
        info.directory -> "assets/models"
        info.filename -> "player.fbx"
        info.basename -> "player"
        info.extension -> "fbx"
*/

#ifndef H_FILE_SYSTEM
#define H_FILE_SYSTEM

#include "typedefs.h"

typedef struct {
  char directory[256];
  char filename[128];
  char basename[128];
  char extension[32];
} FsPathInfo;

FsPathInfo fs_path_parse(const char *path);

#endif
