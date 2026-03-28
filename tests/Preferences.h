#ifndef MOCK_PREFS_H
#define MOCK_PREFS_H
class Preferences {
public:
    void begin(const char*, bool) {}
    void end() {}
    void putFloat(const char*, float) {}
    float getFloat(const char*, float def) { return def; }
    void putUChar(const char*, unsigned char) {}
    unsigned char getUChar(const char*, unsigned char def) { return def; }
};
#endif
