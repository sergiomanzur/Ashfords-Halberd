#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// Return 1 for an unmodified USA retail disc; otherwise a bounded diagnostic.
int ashford_validate_disc(const char* utf8_path, char* error, size_t capacity);
#ifdef __cplusplus
}
#endif
