#pragma once

// Included by main.cpp after its DAC/client state declarations.
// No interrupt handler touches SPI. Ethernet wait hooks only sample GPIO/RAM.
constexpr uint8_t NETWORK_RESET_BUTTON = A1;
constexpr uint8_t SAFE_ZERO_BUTTON = A2;
constexpr uint8_t ETHERNET_CS = 10;
constexpr uint8_t SD_CS = 4;
constexpr uint32_t ZERO_CODE = 131072; // Offset binary, nominal +/-10 V references.
// One 20/2^18 V LSB every 7630 us: <= 0.01 V/s, without catch-up jumps.
constexpr uint32_t SAFE_STEP_US = 7630;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 25;

struct RecoveryButton {
    uint8_t pin;
    bool raw = false;
    bool down = false;
    uint32_t changedAt = 0;
    explicit RecoveryButton(uint8_t p) : pin(p) {}
    bool sample(uint32_t now) {
        bool pressed = digitalRead(pin) == LOW;
        if (pressed != raw) { raw = pressed; changedAt = now; }
        if (down != raw && uint32_t(now - changedAt) >= BUTTON_DEBOUNCE_MS) {
            down = raw;
            return down; // One event per press; release rearms it.
        }
        return false;
    }
};

RecoveryButton networkButton(NETWORK_RESET_BUTTON), safeButton(SAFE_ZERO_BUTTON);
bool networkResetRequested = false, safeZeroRequested = false;
bool networkFault = false, networkEnabled = false;
bool safeZeroActive = false, safeZeroLatched = false;
bool safeZeroFailed = false;
uint32_t safeCodes[4] = {};
uint32_t safeLastStep = 0;
uint32_t safeStepPeriod = SAFE_STEP_US;
constexpr uint32_t NETWORK_HEALTH_MS = 250;
constexpr uint32_t NETWORK_RETRY_MS = 5000;
// Resetting the PHY restarts link negotiation. Give it time to establish its
// first link instead of treating startup LinkOFF as an established-link loss.
constexpr uint32_t NETWORK_LINK_STARTUP_MS = 10000;
uint32_t networkLastHealth = 0, networkLastRestart = 0;
uint32_t networkReadyAt = 0;
bool networkLinkSeenUp = false;

#include "BenchControls.h"

void pollRecoveryButtons() {
    uint32_t now = millis();
    if (safeButton.sample(now)) safeZeroRequested = true;
    if (networkButton.sample(now)) {
        // A press during a safe ramp is discarded, never queued to rearm later.
        if (!safeZeroRequested && !safeZeroActive && !safeButton.raw && !safeButton.down)
            networkResetRequested = true;
    }
    pollBenchInput();
}

// Called inside Ethernet SPI transactions: do not log, reset, or access DACs here.
bool dacNetworkCancelled() {
    pollRecoveryButtons();
    return networkFault || networkResetRequested || safeZeroRequested || safeZeroLatched;
}
void dacNetworkFault() { networkFault = true; }

void forgetNetworkClient() {
    // Do not wait for FIN or perform SPI against a failed Ethernet controller.
    telnetClient = EthernetClient();
    clientConnected = false;
    inputPos = 0;
    inputLine[0] = '\0';
    inputDiscarding = false;
    networkEnabled = false;
}

bool restartNetwork() {
    networkLastRestart = networkLastHealth = millis();
    forgetNetworkClient();
    networkResetRequested = false;
    networkFault = false;
    networkLinkSeenUp = false;
    safeZeroLatched = false; // Only called after completion for rearming.
    safeZeroFailed = false;
    Ethernet.reset(); // Clear ALL socket caches and force chip reset/detection.
    Ethernet.init(ETHERNET_CS);
    Ethernet.begin(mac, IPAddress(192,168,1,200));
    if (dacNetworkCancelled() || Ethernet.hardwareStatus() == EthernetNoHardware) {
        networkFault = true;
        DacLog.println("Network offline; local buttons remain available.");
        return false;
    }
    byte actualMac[6];
    Ethernet.MACAddress(actualMac);
    if (memcmp(mac, actualMac, sizeof(actualMac)) != 0 ||
        Ethernet.localIP() != IPAddress(192,168,1,200)) {
        networkFault = true;
        DacLog.println("Network configuration readback failed.");
        return false;
    }
    Ethernet.setRetransmissionTimeout(100);
    Ethernet.setRetransmissionCount(2);
    telnetServer.begin();
    networkEnabled = !dacNetworkCancelled() && bool(telnetServer);
    if (networkEnabled) {
        // Start the grace period AFTER blocking controller initialization.
        networkReadyAt = networkLastHealth = millis();
        networkLinkSeenUp = Ethernet.linkStatus() == LinkON;
        networkEnabled = !dacNetworkCancelled();
    }
    if (!networkEnabled) networkFault = true;
    DacLog.println(networkEnabled ? "Network ready: 192.168.1.200:23." : "Network listener failed/cancelled.");
    return networkEnabled;
}

// Health checks run only in foreground, between complete SPI transactions.
void serviceNetworkHealth() {
    if (!networkEnabled || dacNetworkCancelled()) return;
    uint32_t now = millis();
    if (uint32_t(now - networkLastHealth) < NETWORK_HEALTH_MS) return;
    networkLastHealth = now;
    EthernetLinkStatus link = Ethernet.linkStatus();
    if (link == LinkON) networkLinkSeenUp = true;
    if (link == LinkOFF && (networkLinkSeenUp ||
        uint32_t(now - networkReadyAt) >= NETWORK_LINK_STARTUP_MS)) {
        networkFault = true;
        DacLog.println(networkLinkSeenUp ? "Network link lost; recovery pending."
                                        : "Network link startup timed out; retrying.");
        return;
    }
    byte actualMac[6];
    Ethernet.MACAddress(actualMac);
    if (memcmp(actualMac, mac, sizeof(actualMac)) != 0 ||
        Ethernet.localIP() != IPAddress(192,168,1,200)) networkFault = true;
}

void serviceNetworkRecovery() {
    // Never restart inside a DAC handler, or silently rearm the safe-zero latch.
    if (!networkFault || dacBusy || safeZeroActive || safeZeroLatched || safeZeroRequested) return;
    if (uint32_t(uint32_t(millis()) - networkLastRestart) >= NETWORK_RETRY_MS)
        restartNetwork();
}

void beginSafeZero() {
    safeZeroRequested = false;
    networkResetRequested = false;
    safeZeroLatched = true;
    safeZeroFailed = false;
    forgetNetworkClient();
    // This runs only after the interrupted handler has returned/released SPI.
    // Do not INIT, CLR, RESET, or change power: those could step the outputs.
    dacBusy = true;
    safeStepPeriod = SAFE_STEP_US;
    for (uint8_t i = 0; i < 4; ++i) {
        uint32_t period = (channelCalibration[i].stepPicovolts + 9999) / 10000;
        if (period > safeStepPeriod) safeStepPeriod = period;
        if (!AD5780_ReadDacValueChecked(static_cast<unsigned char>(DAC_sync[i]),
                                       &safeCodes[i], &AD5780_spi_settings)) {
            // Validate ALL starting values before writing any channel. An
            // unknown starting voltage cannot be ramped without risking a jump.
            safeZeroActive = false;
            safeZeroFailed = true;
            dacBusy = false;
            DacLog.println("SAFE ZERO blocked: invalid DAC readback; latched.");
            return;
        }
    }
    safeLastStep = micros();
    safeZeroActive = true;
    DacLog.println("SAFE ZERO: all channels ramping at 0.01 V/s.");
}

void serviceSafeZero() {
    uint32_t now = micros();
    if (uint32_t(now - safeLastStep) < safeStepPeriod) return;
    // No accumulated catch-up after a delay: each output moves by at most 1 LSB.
    safeLastStep = now;
    bool remaining = false;
    for (uint8_t i = 0; i < 4; ++i) {
        const uint32_t target = channelCalibration[i].zeroCode;
        if (safeCodes[i] != target) {
            if (safeCodes[i] < target) ++safeCodes[i];
            else --safeCodes[i];
            AD5780_SetDacValue(static_cast<unsigned char>(DAC_sync[i]), safeCodes[i], &AD5780_spi_settings);
        }
        remaining |= safeCodes[i] != target;
    }
    if (!remaining) {
        safeZeroActive = false; dacBusy = false;
        DacLog.println("At zero. Release buttons; press NETWORK to rearm.");
    }
    // Keep safeZeroLatched: a fresh network-button press explicitly rearms control.
}

// Returns true when normal network/command work must not execute this loop.
bool servicePhysicalControls() {
    pollRecoveryButtons();
    serviceBenchFault();
    if (safeZeroRequested) {
        if (!safeZeroActive) beginSafeZero();
        else safeZeroRequested = false; // Repeated press doesn't restart/slow the ramp.
    }
    if (safeZeroActive) { serviceSafeZero(); return true; }
    if (networkResetRequested) {
        restartNetwork();
        return true; // Never execute previously buffered commands after a reset.
    }
    if (networkFault) forgetNetworkClient();
    return safeZeroLatched || !networkEnabled;
}
