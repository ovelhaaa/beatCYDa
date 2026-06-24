#include "../src/ui/core/UiUtils.h"
#include <cassert>
#include <cstring>
#include <iostream>

void test_safe_snprintf_normal() {
    char buffer[16];
    int ret = ui::utils::safe_snprintf(buffer, sizeof(buffer), "Hello %d", 123);
    assert(ret == 9);
    assert(std::strcmp(buffer, "Hello 123") == 0);
}

void test_safe_snprintf_truncation() {
    char buffer[8];
    // "Hello 123" is 9 chars + null = 10. Buffer is 8.
    int ret = ui::utils::safe_snprintf(buffer, sizeof(buffer), "Hello %d", 123);
    assert(ret == 9);
    // Should be "Hell..." (total 7 chars + null = 8)
    assert(std::strcmp(buffer, "Hell...") == 0);
}

void test_safe_snprintf_small_buffer() {
    char buffer[3];
    int ret = ui::utils::safe_snprintf(buffer, sizeof(buffer), "Hello");
    assert(ret == 5);
    // Buffer too small for "...", should just be null terminated
    assert(buffer[2] == '\0');
}

void test_safe_snprintf_null() {
    int ret = ui::utils::safe_snprintf(nullptr, 10, "Hello");
    assert(ret == -1);
}

int main() {
    test_safe_snprintf_normal();
    test_safe_snprintf_truncation();
    test_safe_snprintf_small_buffer();
    test_safe_snprintf_null();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
