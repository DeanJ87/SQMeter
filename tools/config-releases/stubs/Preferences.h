#pragma once
#include <cstddef>
#include <string>
struct String
{
    std::string s;
    String(const char *c = "") : s(c) {}
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    bool isEmpty() const { return s.empty(); }
};
class Preferences
{
  public:
    bool begin(const char *, bool = false) { return true; }
    void end() {}
    String getString(const char *, const char *d = "") { return String(d); }
    size_t putString(const char *, const char *) { return 1; }
    bool remove(const char *) { return true; }
    bool isKey(const char *) { return false; }
    size_t getBytesLength(const char *) { return 0; }
    size_t getBytes(const char *, void *, size_t) { return 0; }
    size_t putBytes(const char *, const void *, size_t) { return 0; }
    bool clear() { return true; }
};
