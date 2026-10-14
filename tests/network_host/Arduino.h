#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
using byte = uint8_t;
constexpr int HIGH=1, LOW=0, OUTPUT=1;
inline uint64_t clockUs = 0;
inline uint32_t millis() { return uint32_t(clockUs / 1000); }
inline uint32_t micros() { return uint32_t(clockUs); }
inline void delay(unsigned ms) { clockUs += uint64_t(ms)*1000; }
inline void delayMicroseconds(unsigned us) { clockUs += us; }
inline void pinMode(int, int) {}
void digitalWrite(int, int);
class IPAddress {
    uint8_t bytes[4] = {};
public:
    IPAddress() = default;
    IPAddress(uint32_t v) { std::memcpy(bytes,&v,4); }
    IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes{a,b,c,d} {}
    uint8_t& operator[](unsigned n) { return bytes[n]; }
    const uint8_t& operator[](unsigned n) const { return bytes[n]; }
    uint8_t* raw_address() { return bytes; }
};
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) { return 1; }
    virtual size_t write(const uint8_t*,size_t n) { return n; }
};
