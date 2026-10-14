#pragma once
#include "Arduino.h"
enum BitOrder { LSBFIRST, MSBFIRST };
constexpr int SPI_MODE0 = 0, SPI_MODE1 = 1, SPI_MODE2 = 2, SPI_MODE3 = 3;
struct SPISettings {
    unsigned long freq; BitOrder order; int mode;
    SPISettings(unsigned long f = 4000000, BitOrder o = MSBFIRST, int m = SPI_MODE0): freq(f), order(o), mode(m) {}
    BitOrder getBitOrder() const { return order; }
    unsigned long getClockFreq() const { return freq; }
    int getDataMode() const { return mode; }
};
struct SPIClass {
    bool started = false, active = false, badFrame = false, invalidPin = false;
    bool corruptReads = false;
    int selected = -1, count = 0, transactions = 0, writes = 0;
    uint32_t word = 0, registers[16][8] = {}, pending[16] = {};
    int writePins[4096] = {}; uint32_t codes[4096] = {};
    void begin() { started = true; }
    void beginTransaction(const SPISettings&) {
        if (!started || active) badFrame = true;
        active = true; transactions++;
    }
    void endTransaction() { if (!active || selected != -1) badFrame = true; active = false; }
    uint8_t transfer(uint8_t v) {
        if (!active || selected < 0) { badFrame = true; return 0; }
        word = (word << 8) | v; count++; return 0;
    }
    void transfer(void* ptr, size_t n) {
        auto p = static_cast<uint8_t*>(ptr);
        uint32_t response = selected < 0 || corruptReads ? 0 : pending[selected];
        for (size_t i = 0; i < n; i++) { transfer(p[i]); p[i] = uint8_t(response >> (8 * (n - i - 1))); }
    }
    void select(int pin, int level) {
        if (pin != 2 && pin != 3 && pin != 5 && pin != 6) {
            if (active) invalidPin = true;
            return;
        }
        if (level == LOW) { selected = pin; word = 0; count = 0; }
        else if (selected == pin) {
            if (count != 3) badFrame = true;
            unsigned reg = (word >> 20) & 7;
            if (word & (1u << 23)) pending[pin] = registers[pin][reg];
            else if (reg) {
                registers[pin][reg] = word & 0xFFFFF;
                if (reg == 1) {
                    if (writes < 4096) { writePins[writes] = pin; codes[writes] = (word >> 2) & 0x3FFFF; }
                    writes++;
                }
            }
            selected = -1;
        }
    }
};
inline SPIClass SPI;
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int value) { SPI.select(pin, value); }
