#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// USB debug output must never hold up physical controls when a host stops reading.
// There is one foreground writer; only submit a chunk that already fits in TX.
struct DiagnosticLog {
    void begin(unsigned long baud) { Serial.begin(baud); }
    void print(const char* text) {
        size_t n = strlen(text);
        if (n && Serial.availableForWrite() >= static_cast<int>(n))
            Serial.write(reinterpret_cast<const uint8_t*>(text), n);
    }
    void print(char c) { char text[2] = {c, 0}; print(text); }
    void print(char* text) { print(static_cast<const char*>(text)); }
    template<class T> void print(T value) {
        char text[32];
        snprintf(text, sizeof(text), "%ld", static_cast<long>(value));
        print(text);
    }
    void println() { print("\r\n"); }
    template<class T> void println(T value) { print(value); println(); }
};
static DiagnosticLog DacLog;
