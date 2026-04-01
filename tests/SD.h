#ifndef SD_H
#define SD_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

#define FILE_READ 0

class File {
public:
    std::vector<uint8_t> buffer;
    size_t pos = 0;
    bool is_open = false;

    File() = default;
    File(std::vector<uint8_t> data) : buffer(std::move(data)), is_open(true) {}

    bool operator!() const { return !is_open; }
    operator bool() const { return is_open; }

    size_t read(uint8_t* buf, size_t size) {
        if (!is_open) return 0;
        size_t toRead = std::min(size, buffer.size() - pos);
        std::memcpy(buf, buffer.data() + pos, toRead);
        pos += toRead;
        return toRead;
    }

    bool seek(size_t newPos) {
        if (!is_open) return false;
        if (newPos > buffer.size()) return false;
        pos = newPos;
        return true;
    }

    size_t position() const { return pos; }
    bool available() const { return is_open && (pos < buffer.size()); }
    void close() { is_open = false; }
};

struct SDMock {
    std::vector<uint8_t> currentFile;
    File open(const char* path, int mode) {
        return File(currentFile);
    }
};

extern SDMock SD;

#endif
