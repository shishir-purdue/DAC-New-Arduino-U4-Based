#include <iostream>
#include <string>
#include <new>
#include <cassert>
#include <cstdlib>
static size_t allocations = 0;
void* operator new(size_t n) { allocations++; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

// Compile the actual checked-out firmware, not a Python rewrite of it.
#define DAC_BENCH_CONTROLS 1
#define DAC_HOST_TEST 1
#include "../../Software/src/main.cpp"
#include "../../Software/lib/AD5780/AD5780.cpp"
#include "../../Software/lib/AD5780/Communication.cpp"

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void command(const char* text) {
    char buffer[512]; std::strncpy(buffer, text, sizeof(buffer) - 1); buffer[511] = 0;
    processCommand(buffer);
}
int owner = -1, intruder = -1;
void attachOwner() {
    owner = connectFake(); sockets[owner].accepted = true;
    telnetClient = EthernetClient(owner); clientConnected = true;
    clientLastActivity = millis();
}
void pump(int n = 512) { for (int i = 0; i < n; i++) loop(); }
void dropOwner() {
    sockets[owner].connected = false; sockets[owner].pos = sockets[owner].length;
    delayHook = nullptr;
}
void addIntruder() { intruder = connectFake(); delayHook = nullptr; }
void injectRead() { feedFake(owner, "READ 2\n"); delayHook = nullptr; }
bool earlyReply = false;
void inspectReply() {
    if (delayCalls < 2) return; // Initial readback must be validated before acceptance.
    earlyReply = sockets[owner].tx.contains("RAMP") || sockets[owner].tx.contains("OK");
    feedFake(owner,"STATUS\n");
    delayHook = nullptr;
}
#include "button_tests.h"

void run(const std::string& id) {
    setup();
    if (id.size() > 1 && id[0] == 'B') { runButtons(id); return; }
    if (id == "Mem-1") {
        attachOwner();
        const char* commands[] = {"ID", "HELP", "READ 1", "SET 1 131071", "INIT 1", "RAMP 1 131072 1 0"};
        size_t before = allocations;
        for (int i = 0; i < 1000; i++) for (auto cmd : commands) { delayCalls = 0; command(cmd); }
        require(allocations == before, "Command dispatch allocated heap memory");
    } else if (id == "Mem-2") {
        // A silent client must be accepted, and repeated silent second clients
        // must be closed without exhausting the finite socket pool.
        owner = connectFake(); pump(2);
        require(clientConnected, "Silent first connection was never accepted");
        for (int i = 0; i < 32; i++) {
            intruder = connectFake(); pump(2);
            require(sockets[intruder].stopped, "Silent second client leaked a socket");
        }
        intruder = connectFake(); sockets[intruder].connected = false; pump(2);
        require(sockets[intruder].stopped, "Already-disconnected accepted socket leaked");
        feedFake(owner, "ID\nID\n"); pump();
        require(sockets[owner].connected && sockets[owner].tx.contains("AD5780"), "Active owner was rejected as a second client");
    } else if (id == "Mem-3") {
        attachOwner(); feedFake(owner, "SET 1 "); pump(6);
        require(inputPos > 0, "Test failed to create a partial command");
        dropOwner(); loop();
        require(sockets[owner].stopped && !clientConnected, "Disconnect did not stop and release the client");
        require(inputPos == 0 && inputLine[0] == 0, "Disconnect retained partial command data");
        owner = connectFake("ID\n"); pump();
        require(sockets[owner].tx.contains("AD5780"), "New client could not issue a clean command after disconnect");
    } else if (id == "Robust-1" || id == "C3") {
        attachOwner(); intruder = connectFake("SET 1 200\n"); pump();
        require(sockets[intruder].stopped && SPI.writes == 0, "Second client issued a command or was not stopped");
        delayHook = addIntruder;
        command("RAMP 1 10 1 1");
        require(intruder >= 0 && sockets[intruder].stopped, "Second client was not rejected while ramp was active");
        require(sockets[owner].connected && !dacBusy, "Owner or DAC lock was lost after ramp");
    } else if (id == "Robust-2") {
        attachOwner(); dacBusy = true;
        for (auto cmd : {"INIT", "READ 1", "SET 2 10", "RAMP 3 20 1 1"}) command(cmd);
        require(SPI.transactions == 0 && dacBusy, "Busy handlers touched SPI or released another operation's lock");
        dropOwner(); loop();
        require(dacBusy, "Client cleanup released an in-progress DAC operation's lock");
        intruder = connectFake(); loop();
        require(sockets[intruder].stopped && !clientConnected && dacBusy, "New connection took ownership during an in-progress operation");
        dacBusy = false; attachOwner(); command("SET 1 10");
        require(!dacBusy && SPI.writes == 1, "Successful operation did not release its lock");
        command("RAMP 1 10 0 1"); require(!dacBusy, "Invalid ramp leaked its lock");
        delayHook = dropOwner; command("RAMP 1 100 1 1");
        require(!dacBusy, "Aborted ramp leaked its lock");
    } else if (id == "C1") {
        attachOwner();
        for (auto ch : {"0", "5", "-2", "99"}) for (auto op : {"INIT ", "READ ", "SET ", "RAMP "}) {
            std::string cmd = std::string(op) + ch;
            if (std::string(op) == "SET ") cmd += " 1";
            if (std::string(op) == "RAMP ") cmd += " 1 1 1";
            command(cmd.c_str());
        }
        require(SPI.transactions == 0 && !SPI.invalidPin, "Invalid channel accessed SPI");
        for (auto cmd : {"SET -1 1", "READ -1", "RAMP -1 1 1 1"}) command(cmd);
        require(SPI.transactions == 0, "Channel -1 accepted outside INIT-all alias");
        for (int ch = 1; ch <= 4; ch++) { std::string cmd = "SET " + std::to_string(ch) + " 10"; command(cmd.c_str()); }
        require(SPI.writes == 4 && !SPI.invalidPin, "Valid channel boundary or pin mapping failed");
    } else if (id == "C2") {
        attachOwner(); command("SET 1 10");
        require(SPI.started && !SPI.badFrame, "SPI not started before DAC traffic");
    } else if (id == "C5") {
        attachOwner(); delayHook = dropOwner; command("RAMP 1 100 1 1");
        require(SPI.writes < 100 && !dacBusy, "Ramp did not abort/release lock on disconnect");
        require(Ethernet.maintenance > 0, "Ramp omitted maintenance callback");
        // This covers the implemented partial fix only; M4 covers responsiveness.
    } else if (id == "H2" || id == "H3") {
        attachOwner(); command(id == "H2" ? "RAMP 1 100 -5 1" : "RAMP 1 100 1 -1");
        require(SPI.writes == 0, "Negative ramp argument was accepted and changed the DAC");
        require(largestDelay < 1000000, "Negative delay became a huge unsigned wait");
    } else if (id == "C4") {
        seedCodes(157286,131072,131072,131072); attachOwner();
        feedFake(owner,"SET 1 "); pump(6);
        fakeLinkStatus = LinkOFF; fakeMicros += NETWORK_HEALTH_MS*1000; loop();
        require(networkFault && !clientConnected && inputPos==0 && SPI.writes==0,
                "Link loss retained stale commands or disturbed DAC outputs");
        int resets=networkResetCalls;
        fakeLinkStatus=LinkON; fakeMicros += NETWORK_RETRY_MS*1000; loop();
        require(networkEnabled && !networkFault && networkResetCalls==resets+1 && dacCode(0)==157286,
                "Link return failed automatic recovery or changed DAC output");
        owner=connectFake("ID\n"); pump();
        require(sockets[owner].tx.contains("AD5780"), "Recovered listener did not accept fresh client");
        // Configuration loss also requires a full reset, and retries are bounded.
        fakeMac[0]^=1; fakeMicros+=NETWORK_HEALTH_MS*1000; loop();
        require(networkFault, "Lost controller configuration not detected");
        fakeNetworkMissing=true; fakeMicros+=NETWORK_RETRY_MS*1000; loop();
        resets=networkResetCalls; pump();
        require(networkFault && networkResetCalls==resets, "Missing controller caused a tight retry loop");
        press(A2); fakeMicros+=NETWORK_RETRY_MS*1000; loop();
        require(networkResetCalls==resets && safeZeroLatched, "Automatic recovery overrode safe-zero latch");
    } else if (id == "H4") {
        attachOwner(); command("RAMP 1 2 1 1000000");
        require(largestDelay < 1000000, "One-second step is passed straight to delayMicroseconds; bounded/cooperative wait policy not implemented");
    } else if (id == "H5") {
        SPI.forcedRead=0xA00016; // R/W header must not turn the write into another read.
        AD5780_EnableOutput(2,1,&AD5780_spi_settings);
        require(SPI.registers[2][2]==0x12 && SPI.writes==0 && !SPI.badFrame,
                "EnableOutput failed to strip read header or changed DAC code");
    } else if (id == "H6") {
        char minus[] = "-", plus[] = "+", good[] = "-12";
        require(!isInteger(minus) && !isInteger(plus) && isInteger(good), "Bare '+' or '-' is accepted as an integer");
        attachOwner();
        for (auto cmd : {"SET 1 +", "SET 1 -", "RAMP 1 10 -1 1", "RAMP 1 10 1 -1",
                         "RAMP 1 10 0 1", "RAMP 1 10 262144 1", "RAMP 1 10 1 2147483648",
                         "SET 1 -2147483649", "READ 4294967297", "SET 1 12x"}) command(cmd);
        require(SPI.transactions==0 && !dacBusy, "Malformed/range-invalid argument touched DAC or leaked lock");
        int parsed=7; char max[]="2147483647", min[]="-2147483648", overflow[]="2147483648";
        require(parseInteger(max,parsed) && parsed==INT32_MAX && parseInteger(min,parsed) && parsed==INT32_MIN,
                "Signed 32-bit boundaries rejected");
        require(!parseInteger(overflow,parsed) && parsed==INT32_MIN, "Overflow accepted or changed output parameter");
    } else if (id == "M1") {
        attachOwner(); fakeMicros += 119000000; pump();
        require(clientConnected,"Idle timeout closed an active lease too early");
        feedFake(owner,"ID\n"); pump(); fakeMicros+=119000000; pump();
        require(clientConnected,"Received command failed to renew idle lease");
        fakeMicros+=1000000; pump();
        require(sockets[owner].stopped && !clientConnected && inputPos==0,
                "Silent half-open owner retained after idle timeout");
        intruder = connectFake("ID\n"); pump();
        require(sockets[intruder].tx.contains("AD5780"),"Idle timeout did not free socket for new owner");
        owner=intruder; dacBusy=true; fakeMicros+=130000000; serviceClientConnections();
        require(clientConnected && dacBusy,"Idle timeout interrupted a long running operation");
        dacBusy=false; serviceClientConnections(); require(clientConnected,"No idle grace after long operation");
        fakeMicros=((uint64_t(1)<<32)-1000)*1000; clientLastActivity=millis();
        fakeMicros+=120000000; serviceClientConnections();
        require(!clientConnected,"Idle timeout failed across millis rollover");
    } else if (id == "M2") {
        attachOwner(); command("SET 1 262144");
        require(sockets[owner].tx.contains("WARN") || sockets[owner].tx.contains("ERR"), "Clamped target is echoed but no explicit warning/error reaches client");
        require(dacCode(0)==262143 && sockets[owner].tx.contains("262143"),"Clamped value not reported accurately");
        sockets[owner].tx.clear(); command("SET 1 -1");
        require(dacCode(0)==0 && sockets[owner].tx.contains("WARN"),"Negative target clamp not reported");
        sockets[owner].tx.clear(); command("RAMP 1 262144 262143 0");
        require(dacCode(0)==262143 && sockets[owner].tx.contains("WARN"),"Ramp clamp not reported");
    } else if (id == "M3") {
        attachOwner(); std::string line = "SET 1 42" + std::string(140, ' ') + "BAD\n";
        feedFake(owner, line.c_str()); pump();
        require(SPI.writes == 0, "Overflowed line executed its truncated valid prefix");
        require(sockets[owner].tx.contains("ERR"), "Oversized line did not produce an explicit error");
        feedFake(owner,"SET 1 17\r\n"); pump();
        require(SPI.writes==1 && dacCode(0)==17, "Parser did not recover at the next line");
        // A NUL byte cannot turn a longer malformed line into a valid prefix.
        auto& s=sockets[owner]; const char binary[]={'S','E','T',' ','1',' ','9','\0','x','\n'};
        memcpy(s.rx+s.length,binary,sizeof(binary)); s.length+=sizeof(binary); pump();
        require(SPI.writes==1, "Embedded NUL executed a truncated command");
    } else if (id == "M4") {
        attachOwner(); delayHook = injectRead; command("RAMP 1 10 1 1");
        require(sockets[owner].pos == sockets[owner].length, "Command arriving during ramp was not serviced; RX can accumulate");
        require(sockets[owner].tx.contains("busy"), "Command during ramp was not explicitly rejected");
        require(dacCode(0)==10 && !dacBusy && !SPI.badFrame, "Nested command interrupted ramp/lock/SPI framing");
    } else if (id == "M8") {
        attachOwner(); command("INIT"); command("SET 2 100"); command("READ 2"); command("RAMP 2 105 1 1");
        require(!SPI.badFrame && !SPI.active && !SPI.invalidPin, "SPI transaction framing is unbalanced");
        // Interrupt contention is NOT emulated. Notebook reports REVIEW separately.
    } else if (id == "M9") {
        attachOwner(); seedCodes(100,200,300,400); int before = SPI.writes; command("INIT 1");
        require(SPI.registers[2][1] == (100u << 2) && SPI.writes == before, "INIT overwrote existing output with midscale; startup/transient policy needs approval and scope measurement");
        command("INIT"); require(SPI.writes==before && dacCode(3)==400, "INIT-all changed an active output");
        SPI.registers[2][2]=0x0E; command("INIT 1");
        require(dacCode(0)==131072 && SPI.registers[2][2]==0x12, "Cold clamped channel failed zero initialization");
        before=SPI.writes; command("INIT 1"); require(SPI.writes==before,"Repeated INIT is not idempotent");
        SPI.registers[2][2]=0x02; sockets[owner].tx.clear(); command("INIT 1");
        require(SPI.writes==before && sockets[owner].tx.contains("ERR"), "Active incompatible coding was changed");
    } else if (id == "M10") {
        attachOwner(); command("SET 1 999999999999999999999999999999999999");
        require(SPI.writes == 0, "Out-of-range numeric token was not rejected before DAC access");
    } else if (id == "M11") {
        attachOwner(); delayHook = inspectReply; command("RAMP 1 10 1 1");
        require(earlyReply, "No ramp acknowledgement before the first blocking delay");
        require(sockets[owner].tx.contains("RAMP ACCEPTED") && sockets[owner].tx.contains("RAMP RUNNING"),
                "Acceptance/status unavailable while ramp was running");
        require(!sockets[owner].tx.contains("RAMP COMPLETE"),"Unsolicited completion polluted query stream");
        sockets[owner].tx.clear(); command("STATUS");
        require(sockets[owner].tx.contains("RAMP COMPLETE 1 10"),"STATUS failed to report completion");
        delayCalls=0; delayHook=dropOwner; command("RAMP 1 20 1 1000");
        require(strcmp(rampState,"ABORTED")==0,"Disconnect failed to record aborted state");
    } else if (id == "A1") {
        owner = connectFake("ID\nREAD 1\n"); pump();
        require(sockets[owner].connected && sockets[owner].tx.contains("AD5780"), "Owner disconnected while its own command was still buffered");
    } else if (id == "A2") {
        attachOwner(); SPI.corruptReads = true; command("INIT 1");
        require(sockets[owner].tx.contains("ERR") && !sockets[owner].tx.contains("Initialized"), "INIT reports success even with mismatched register readback");
        require(SPI.writes==0 && !dacBusy,"Bad readback caused DAC writes/lock leak");
        SPI.corruptReads=false; SPI.ignoreControlWrites=true; sockets[owner].tx.clear(); command("INIT 1");
        require(sockets[owner].tx.contains("ERR") && SPI.writes==0,"Control write failure ignored");
        SPI.ignoreControlWrites=false; SPI.ignoreDacWrites=true; sockets[owner].tx.clear(); command("INIT 1");
        require(sockets[owner].tx.contains("ERR") && (SPI.registers[2][2]&AD5780_CTRL_OPGND),
                "DAC write/readback failure released ground clamp");
    } else if (id == "A4") {
        attachOwner();
        // First delay is register read latency. Disconnect on the first step delay.
        delayHook = []() { if (delayCalls >= 2) dropOwner(); };
        command("RAMP 1 100 1 1000");
        require(SPI.writes == 0, "Ramp writes one more output after connection drops during its step delay");
    } else throw std::runtime_error("Unknown host test ID");
}
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: firmware_tests CHECKLIST-ID\n"; return 2; }
    try { run(argv[1]); std::cout << "PASS " << argv[1] << "\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "FAIL " << argv[1] << ": " << e.what() << "\n"; return 1; }
}
