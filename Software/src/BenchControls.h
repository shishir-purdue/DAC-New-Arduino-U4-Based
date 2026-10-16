#pragma once

// Explicitly opt-in USB test interface. Production builds do not consume serial
// commands. Requests share the physical controls' action flags; no ISR/SPI work.
#if defined(DAC_BENCH_CONTROLS) && DAC_BENCH_CONTROLS
char benchInput[40] = {};
uint8_t benchInputPos = 0;
bool benchDiscarding = false;
char benchReply[64] = {};
extern bool dacBenchBadReadback;
bool benchNetworkFaultRequested = false;

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
            } else if (strcmp(benchInput, "BENCH FAULT READBACK") == 0) {
                if (dacBusy || safeZeroRequested || safeZeroActive)
                    queueBenchReply("BENCH ERR DAC BUSY");
                else {
                    dacBenchBadReadback = true;
                    queueBenchReply("BENCH OK FAULT READBACK");
                }
            } else if (strcmp(benchInput, "BENCH FAULT CLEAR") == 0) {
                dacBenchBadReadback = false;
                queueBenchReply("BENCH OK FAULT CLEAR");
            } else if (strcmp(benchInput, "BENCH FAULT NETWORK") == 0) {
                if (safeZeroRequested || safeZeroActive || safeZeroLatched)
                    queueBenchReply("BENCH ERR SAFE ZERO ACTIVE OR LATCHED");
                else {
                    benchNetworkFaultRequested = true;
                    queueBenchReply("BENCH OK FAULT NETWORK");
                }
            } else if (strcmp(benchInput, "BENCH STATUS") == 0) {
                queueBenchReply(safeZeroFailed ? "BENCH STATUS FAILED" :
                    safeZeroActive ? "BENCH STATUS ACTIVE" :
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

// Foreground only, outside Ethernet/DAC transactions. Corrupt the real W5500
// configuration so the normal health check must detect/recover the failure.
void serviceBenchFault() {
    if (benchNetworkFaultRequested) {
        benchNetworkFaultRequested = false;
        if (!safeZeroRequested && !safeZeroActive && !safeZeroLatched)
            Ethernet.setLocalIP(IPAddress(0,0,0,0));
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
inline void serviceBenchFault() {}
#endif
