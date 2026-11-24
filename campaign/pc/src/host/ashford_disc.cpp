// Reuse Ashford's existing ISO identity/inventory checks before the campaign starts.
#include "ashford_disc.h"
#include <cstdio>

#ifdef __ANDROID__

extern "C" int ashford_validate_disc(const char* path, char* error, size_t capacity) {
    (void)path;
    if (error && capacity) {
        std::snprintf(error, capacity, "%s", "Disc validation is unavailable on this platform until Plan 2.");
    }
    return 0;
}

#else

#include "core.hpp"
#include <exception>

extern "C" int ashford_validate_disc(const char* path, char* error, size_t capacity) {
    if (error && capacity) error[0]=0;
    try {
        if (!path || !*path) throw std::runtime_error("Select your USA SLUS-20184 ISO.");
        ashford::validate_disc(ashford::read_disc(ashford::fs::u8path(path)));
        return 1;
    } catch (const std::exception& exception) {
        if (error && capacity) std::snprintf(error,capacity,"%s",exception.what());
        return 0;
    }
}

#endif
