#include "os.h"
#ifdef MACOS
#include "os_macos.c"
#elif defined(LINUX)
#include "os_linux.c"
#endif
