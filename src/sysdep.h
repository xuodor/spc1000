#pragma once

#include <stdint.h>
#include <glob.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIR_PATH "disk"

FILE *ext_fopen(const char *pathname, const char *mode);
void init_fs();
uint32_t get_timestamp_ms();

#ifdef __cplusplus
}
#endif
