#pragma once
#include "Arduino.h"
// available(): any server-owned socket with bytes, even one previously returned.
// accept(): transfers ownership once, including clients that have sent no bytes.
// Contract: arduino-libraries/Ethernet src/EthernetServer.cpp (see tests/README.md).
struct IPAddress {
    int octets[4];
    IPAddress(int a, int b, int c, int d) : octets{a,b,c,d} {}
    operator int() const { return 0; }
    bool operator!=(const IPAddress& other) const { return std::memcmp(octets, other.octets, sizeof(octets)) != 0; }
};
enum EthernetHardwareStatus { EthernetNoHardware, EthernetW5500 };
inline bool fakeNetworkMissing = false;
inline int networkResetCalls = 0, networkCalls = 0;
inline byte fakeMac[6] = {};
inline void (*networkBeginHook)() = nullptr;
struct SocketState {
    bool used = false, connected = false, accepted = false, stopped = false;
    char rx[16384] = {}; size_t pos = 0, length = 0;
    TextBuffer tx;
};
inline SocketState sockets[8];
inline int stopCalls = 0;
inline int connectFake(const char* input = "") {
    for (int i = 0; i < 8; i++) if (!sockets[i].used) {
        sockets[i] = SocketState{}; sockets[i].used = sockets[i].connected = true;
        std::strncpy(sockets[i].rx, input, sizeof(sockets[i].rx) - 1);
        sockets[i].length = std::strlen(sockets[i].rx); return i;
    }
    throw std::runtime_error("No free simulated Ethernet sockets");
}
inline void feedFake(int i, const char* input) {
    auto& s = sockets[i]; size_t n = std::strlen(input);
    if (s.length + n >= sizeof(s.rx)) throw std::runtime_error("Simulated RX capacity exceeded");
    std::memcpy(s.rx + s.length, input, n + 1); s.length += n;
}
class EthernetClient {
public:
    int id = -1;
    EthernetClient(int i = -1): id(i) {}
    explicit operator bool() const { return id >= 0 && sockets[id].used; }
    bool connected() const { return bool(*this) && (sockets[id].connected || available()); }
    int available() const { return bool(*this) ? int(sockets[id].length - sockets[id].pos) : 0; }
    int read() { return available() ? sockets[id].rx[sockets[id].pos++] : -1; }
    void stop() {
        if (id >= 0) { sockets[id].stopped = true; sockets[id].used = sockets[id].connected = false; stopCalls++; }
        id = -1;
    }
    template<class T> void print(T v) { if (*this) sockets[id].tx.print(v); }
    bool operator==(const EthernetClient& other) const { return id == other.id; }
    bool operator!=(const EthernetClient& other) const { return !(*this == other); }
};
class EthernetServer {
public:
    explicit EthernetServer(int) {}
    void begin() {}
    explicit operator bool() const { return !fakeNetworkMissing; }
    EthernetClient available() {
        int found = -1;
        for (int i = 0; i < 8; i++) if (sockets[i].used && !sockets[i].accepted && sockets[i].length > sockets[i].pos) found = i;
        return EthernetClient(found);
    }
    EthernetClient accept() {
        for (int i = 0; i < 8; i++) if (sockets[i].used && !sockets[i].accepted) {
            sockets[i].accepted = true; return EthernetClient(i);
        }
        return EthernetClient();
    }
};
struct EthernetClass {
    int maintenance = 0;
    void reset() {
        networkResetCalls++; networkCalls++;
        for (auto& s : sockets) { s.used = s.connected = false; s.stopped = true; }
    }
    void init(uint8_t) { networkCalls++; }
    void begin(byte* mac, IPAddress) { networkCalls++; digitalWrite(10, HIGH); std::memcpy(fakeMac, mac, 6); if (networkBeginHook) networkBeginHook(); }
    void MACAddress(byte* out) { networkCalls++; std::memcpy(out, fakeMac, 6); }
    EthernetHardwareStatus hardwareStatus() { networkCalls++; return fakeNetworkMissing ? EthernetNoHardware : EthernetW5500; }
    void setRetransmissionTimeout(uint16_t) { networkCalls++; }
    void setRetransmissionCount(uint8_t) { networkCalls++; }
    IPAddress localIP() { networkCalls++; return IPAddress(192,168,1,200); }
    int maintain() { networkCalls++; maintenance++; return 0; }
};
inline EthernetClass Ethernet;
