#include "file_system.h"
#include "lib/string.h"

FsPathInfo fs_path_parse(const char *path) {
  FsPathInfo info = {0};

  const char *filename_start = path;
  for (const char *p = path; *p; p++) {
    if (*p == '/' || *p == '\\') {
      filename_start = p + 1;
    }
  }

  i32 dir_len = (i32)(filename_start - path);
  if (dir_len > 0) {
    i32 copy_len = dir_len > (i32)sizeof(info.directory) - 1
                    ? (i32)sizeof(info.directory) - 1
                    : dir_len;
    memcpy(info.directory, path, copy_len);
    info.directory[copy_len] = '\0';
  }

  str_copy(info.filename, (char*)filename_start, sizeof(info.filename));

  str_copy(info.basename, (char*)filename_start, sizeof(info.basename));

  for (char *p = info.basename; *p; p++) {
    if (*p == '.') {
      *p = '\0';

      const char *ext_start = filename_start + (p - info.basename) + 1;
      str_copy(info.extension, (char*)ext_start, sizeof(info.extension));
      break;
    }
  }

  return info;
}
