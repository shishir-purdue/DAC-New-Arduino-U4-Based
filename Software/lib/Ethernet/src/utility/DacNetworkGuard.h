#pragma once
#include <Arduino.h>

// DAC Box hooks. Must only sample GPIO/update RAM: callers may own SPI.
bool dacNetworkCancelled();
void dacNetworkFault();

class DacNetworkWait {
    uint32_t started = millis();
    uint32_t polls = 0;
    uint32_t limit;
public:
    explicit DacNetworkWait(uint32_t timeoutMs = 100) : limit(timeoutMs) {}
    bool expired() {
        if (dacNetworkCancelled()) return true;
        // The iteration bound also covers a stalled timebase.
        if (uint32_t(millis() - started) >= limit || ++polls >= 100000) {
            dacNetworkFault();
            return true;
        }
        return false;
    }
};
