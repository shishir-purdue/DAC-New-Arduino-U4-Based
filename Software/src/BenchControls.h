#pragma once

// Explicitly opt-in USB test interface. Production builds do not consume serial
// commands. Requests share the physical controls' action flags; no ISR/SPI work.
#if defined(DAC_BENCH_CONTROLS) && DAC_BENCH_CONTROLS
char benchInput[40] = {};
uint8_t benchInputPos = 0;
bool benchDiscarding = false;
char benchReply[64] = {};

void queueBenchReply(const char* text) {
    snprintf(benchReply, sizeof(benchReply), "%s\r\n", text);
}

// May run from an Ethernet wait hook while SPI is owned. Only USB RX and RAM;
// response output and the action itself are deferred to foreground service.
void pollBenchInput() {
    if (benchReply[0]) return;
    unsigned budget = 64;
    while (budget-- && Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (benchDiscarding) {
                benchDiscarding = false; benchInputPos = 0;
                queueBenchReply("BENCH ERR INVALID LINE");
                return;
            }
            if (!benchInputPos) continue;
            benchInput[benchInputPos] = '\0'; benchInputPos = 0;
            if (strcmp(benchInput, "BENCH SAFEZERO") == 0) {
                safeZeroRequested = true;
                queueBenchReply("BENCH OK SAFEZERO");
            } else if (strcmp(benchInput, "BENCH NETWORK") == 0) {
                if (safeZeroRequested || safeZeroActive || safeButton.raw || safeButton.down)
                    queueBenchReply("BENCH ERR SAFE ZERO ACTIVE OR HELD");
                else {
                    networkResetRequested = true;
                    queueBenchReply("BENCH OK NETWORK");
                }
            } else if (strcmp(benchInput, "BENCH STATUS") == 0) {
                queueBenchReply(safeZeroActive ? "BENCH STATUS ACTIVE" :
                    safeZeroLatched ? "BENCH STATUS LATCHED" :
                    networkEnabled && !networkFault ? "BENCH STATUS READY" : "BENCH STATUS OFFLINE");
            } else queueBenchReply("BENCH ERR UNKNOWN COMMAND");
            return;
        } else if (!benchDiscarding) {
            if (c == '\0' || benchInputPos >= sizeof(benchInput)-1) {
                benchDiscarding = true; benchInputPos = 0;
            } else benchInput[benchInputPos++] = c;
        }
    }
}

void flushBenchReply() {
    size_t length = strlen(benchReply);
    if (length && Serial.availableForWrite() >= int(length)) {
        Serial.write(reinterpret_cast<const uint8_t*>(benchReply), length);
        benchReply[0] = '\0';
    }
}
#else
inline void pollBenchInput() {}
inline void flushBenchReply() {}
#endif
