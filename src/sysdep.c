#include "sysdep.h"
#include <time.h>
#include <unistd.h>

void init_fs() {
  chdir(DIR_PATH);
}

FILE *ext_fopen(const char *pathname, const char *mode) {
  return fopen(pathname, mode);
}

uint32_t get_timestamp_ms() {
 struct timespec tms;

  /* POSIX.1-2008 way */
  if (clock_gettime(CLOCK_REALTIME, &tms))
      return -1;

  int64_t millis = tms.tv_sec * 1000;
  millis += tms.tv_nsec/1000000;
  return millis;
}
