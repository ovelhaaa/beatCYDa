#pragma once
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace ui {
namespace utils {

/**
 * @brief A safe wrapper for snprintf that handles null pointers and truncation.
 *
 * If truncation occurs, it appends "..." to the end of the buffer.
 */
inline int safe_snprintf(char* buffer, size_t size, const char* format, ...) {
    if (buffer == nullptr || size == 0) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, size, format, args);
    va_end(args);

    if (ret >= static_cast<int>(size)) {
        // Truncation occurred. Indicate by appending "..."
        if (size >= 4) {
            buffer[size - 4] = '.';
            buffer[size - 3] = '.';
            buffer[size - 2] = '.';
            buffer[size - 1] = '\0';
        } else if (size > 0) {
            buffer[size - 1] = '\0';
        }
    } else if (ret < 0) {
        // Encoding error
        if (size > 0) {
            buffer[0] = '\0';
        }
    }

    return ret;
}

} // namespace utils
} // namespace ui
