#pragma once
// Deterministic host doubles. No real networking, GPIO, sleeping, or allocation
// in the doubles' command hot paths. These are NOT an Arduino core emulator.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <stdexcept>
using byte = unsigned char;
constexpr int OUTPUT = 1, HIGH = 1, LOW = 0;
constexpr int INPUT_PULLUP = 2, A1 = 15, A2 = 16;
inline int gpioInputs[32] = {};
inline int gpioModes[32] = {}, gpioOutputs[32] = {};
inline int digitalRead(int pin) { return gpioInputs[pin]; }
inline uint64_t fakeMicros = 0;
inline uint32_t largestDelay = 0;
inline unsigned delayCalls = 0;
inline void (*delayHook)() = nullptr;
inline int serialWriteSpace = 65535;
struct TextBuffer {
    char data[65536] = {};
    size_t size = 0;
    void clear() { size = 0; data[0] = 0; }
    void print(const char* s) {
        size_t n = std::strlen(s);
        if (n > sizeof(data) - size - 1) n = sizeof(data) - size - 1;
        std::memcpy(data + size, s, n); size += n; data[size] = 0;
    }
    void print(char* s) { print(static_cast<const char*>(s)); }
    void print(char c) { char s[] = {c, 0}; print(s); }
    template<class T> void print(T n) {
        char s[48]; std::snprintf(s, sizeof(s), "%lld", (long long)n); print(s);
    }
    template<class T> void println(T v) { print(v); print("\r\n"); }
    void println() { print("\r\n"); }
    void begin(int) {}
    int availableForWrite() const { return serialWriteSpace; }
    size_t write(const uint8_t* p, size_t n) {
        if (n > static_cast<size_t>(serialWriteSpace)) throw std::runtime_error("Blocking serial write attempted");
        for (size_t i = 0; i < n; ++i) print(static_cast<char>(p[i]));
        return n;
    }
    bool contains(const char* s) const { return std::strstr(data, s) != nullptr; }
};
inline TextBuffer Serial;
void pinMode(int, int);
void digitalWrite(int, int);
inline void delayMicroseconds(unsigned int us) {
    if (++delayCalls > 10000) throw std::runtime_error("Ramp exceeded 10000 simulated delays; possible runaway");
    if (us > largestDelay) largestDelay = us;
    fakeMicros += us;
    if (delayHook) delayHook();
}
inline void delay(unsigned long ms) { fakeMicros += uint64_t(ms) * 1000; if (delayHook) delayHook(); }
inline unsigned long millis() { return (unsigned long)(fakeMicros / 1000); }
inline unsigned long micros() { return (unsigned long)fakeMicros; }
