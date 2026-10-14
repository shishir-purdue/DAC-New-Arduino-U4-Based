#pragma once
#include "../../Software/lib/Ethernet/src/utility/DacNetworkGuard.h"

void advanceButtons(uint32_t us) { fakeMicros += us; servicePhysicalControls(); }
void press(int pin) {
    gpioInputs[pin] = LOW; servicePhysicalControls();
    advanceButtons(BUTTON_DEBOUNCE_MS * 1000);
}
void release(int pin) {
    gpioInputs[pin] = HIGH; servicePhysicalControls();
    advanceButtons(BUTTON_DEBOUNCE_MS * 1000);
}
uint32_t dacCode(int channel) { return SPI.registers[DAC_sync[channel]][1] >> 2; }
void seedCodes(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t codes[] = {a,b,c,d};
    for (int i = 0; i < 4; ++i) {
        SPI.registers[DAC_sync[i]][1] = codes[i] << 2;
        SPI.registers[DAC_sync[i]][2] = 0x12;
    }
}
void runButtons(const std::string& id) {
    if (id == "B1") {
        seedCodes(157286,100,262143,0); attachOwner();
        feedFake(owner, "SET 1 "); pump(6);
        int resets = networkResetCalls;
        gpioInputs[A1] = LOW; advanceButtons(1000);
        gpioInputs[A1] = HIGH; advanceButtons(1000);
        gpioInputs[A1] = LOW; advanceButtons(24000);
        require(networkResetCalls == resets, "Bounce triggered network reset");
        advanceButtons(25000);
        require(networkResetCalls == resets + 1, "Stable network button failed");
        require(!clientConnected && !telnetClient && inputPos == 0 && inputLine[0] == 0, "Reset retained client/partial command");
        require(SPI.writes == 0 && dacCode(0) == 157286, "Network reset touched DAC");
        pump(100); require(networkResetCalls == resets + 1, "Held button retriggered reset");
        owner = connectFake("ID\n"); pump();
        require(sockets[owner].tx.contains("AD5780"), "Fresh connection failed after reset");
        release(A1); press(A1); require(networkResetCalls == resets + 2, "Release did not rearm network button");
    } else if (id == "B2" || id == "B9") {
        seedCodes(ZERO_CODE+10,ZERO_CODE,ZERO_CODE,ZERO_CODE); attachOwner();
        delayHook = [ ]() {
            if (delayCalls >= 2) {
                gpioInputs[A1] = LOW; pollRecoveryButtons();
                fakeMicros += 25000; delayHook = nullptr;
            }
        };
        if (id == "B9") delayHook = []() {
            if (delayCalls >= 2) {
                gpioInputs[A2] = LOW; pollRecoveryButtons();
                fakeMicros += 25000; delayHook = nullptr;
            }
        };
        command("RAMP 1 200000 10 1000000000");
        require(!dacBusy && SPI.writes == 0, "Button during long delay allowed another ramp write");
        require(largestDelay <= 1000, "Normal ramp did not bound cancellation latency");
        servicePhysicalControls();
        require(id == "B2" ? networkEnabled : safeZeroActive, "Button action not serviced after ramp unwound");
        require(!SPI.active && !SPI.badFrame, "Button nested SPI transactions");
    } else if (id == "B3") {
        seedCodes(ZERO_CODE+2,ZERO_CODE-2,ZERO_CODE, ZERO_CODE+1);
        press(A2);
        require(SPI.writes == 0, "Safe button jumped immediately to zero");
        advanceButtons(SAFE_STEP_US-1); require(SPI.writes == 0, "Safe ramp stepped too early");
        advanceButtons(1);
        require(dacCode(0)==ZERO_CODE+1 && dacCode(1)==ZERO_CODE-1 && dacCode(2)==ZERO_CODE && dacCode(3)==ZERO_CODE, "Channels did not approach zero concurrently");
        advanceButtons(1000000); // A stall cannot produce a catch-up jump.
        require(dacCode(0)==ZERO_CODE && dacCode(1)==ZERO_CODE && !safeZeroActive && safeZeroLatched, "Zero ramp did not finish/latch");
        require(SPI.writes == 5 && !SPI.badFrame, "Unnecessary DAC writes or SPI overlap");
    } else if (id == "B4") {
        seedCodes(0,262143,157286,ZERO_CODE);
        press(A2); uint64_t start = fakeMicros;
        uint32_t before[4] = {0,262143,157286,ZERO_CODE};
        for (int n=0; n<131072; ++n) {
            delayCalls = 0; advanceButtons(SAFE_STEP_US);
            for (int i=0; i<4; ++i) {
                uint32_t expected = before[i] == ZERO_CODE ? ZERO_CODE : before[i] + (before[i] < ZERO_CODE ? 1 : -1);
                require(dacCode(i) == expected, "Safe ramp skipped/overshot a code"); before[i] = expected;
            }
        }
        require(!safeZeroActive && !dacBusy && fakeMicros-start >= 1000000000ULL, "Full-range ramp too fast or failed to complete");
    } else if (id == "B5") {
        fakeNetworkMissing = true; restartNetwork();
        require(networkFault && !networkEnabled, "Missing Ethernet incorrectly reported ready");
        seedCodes(ZERO_CODE+1,ZERO_CODE-1,ZERO_CODE,ZERO_CODE);
        int calls = networkCalls; press(A2); advanceButtons(SAFE_STEP_US);
        require(networkCalls == calls && !safeZeroActive && dacCode(0)==ZERO_CODE && dacCode(1)==ZERO_CODE, "Safe ramp used unavailable Ethernet");
    } else if (id == "B6") {
        DacNetworkWait wait;
        fakeMicros += 100000;
        require(wait.expired() && networkFault, "Driver wait timeout not latched");
        networkFault = false;
        DacNetworkWait cancelled;
        gpioInputs[A2] = LOW; require(!cancelled.expired(), "Undebounced button triggered");
        fakeMicros += 25000;
        require(cancelled.expired() && safeZeroRequested && SPI.transactions == 0, "Driver hook failed to latch button without SPI");
    } else if (id == "B7") {
        seedCodes(ZERO_CODE+10,ZERO_CODE,ZERO_CODE,ZERO_CODE); int resets = networkResetCalls;
        gpioInputs[A1] = gpioInputs[A2] = LOW; servicePhysicalControls(); advanceButtons(25000);
        require(safeZeroActive && networkResetCalls == resets, "Simultaneous network reset overrode safe ramp");
        command("SET 1 262143"); require(SPI.writes == 0, "Remote command overrode safe latch");
        for (int i=0; i<10; ++i) advanceButtons(SAFE_STEP_US);
        require(!safeZeroActive && safeZeroLatched, "Completion automatically rearmed network");
        release(A2); advanceButtons(1000000);
        require(networkResetCalls == resets && safeZeroLatched, "Old network press rearmed after completion");
        release(A1); press(A1);
        require(networkEnabled && !safeZeroLatched && dacCode(0)==ZERO_CODE, "Fresh reset failed to rearm at zero");
    } else if (id == "B8") {
        seedCodes(ZERO_CODE+1,ZERO_CODE,ZERO_CODE,ZERO_CODE);
        networkBeginHook = []() {
            gpioInputs[A2] = LOW; pollRecoveryButtons(); fakeMicros += 25000; pollRecoveryButtons();
        };
        press(A1); networkBeginHook = nullptr;
        require(!networkEnabled && safeZeroRequested, "Safe button lost during network reinitialization");
        servicePhysicalControls(); advanceButtons(SAFE_STEP_US);
        require(dacCode(0)==ZERO_CODE && safeZeroLatched, "Recovery cancellation did not proceed to zero");
    } else if (id == "B10") {
        seedCodes(ZERO_CODE+1,ZERO_CODE,ZERO_CODE,ZERO_CODE);
        fakeMicros = (uint64_t(1)<<32)-1000; press(A2);
        safeLastStep = 0xFFFFF000u; fakeMicros = (uint64_t(1)<<32)+4000;
        servicePhysicalControls(); require(!safeZeroActive && dacCode(0)==ZERO_CODE, "micros wrap broke safe ramp");
        release(A2); networkFault = false; safeZeroLatched = false;
        fakeMicros = ((uint64_t(1)<<32)-10)*1000;
        gpioInputs[A1] = LOW; pollRecoveryButtons(); fakeMicros += 30000; pollRecoveryButtons();
        require(networkResetRequested, "millis wrap broke button debounce");
    } else if (id == "B11") {
        attachOwner(); serialWriteSpace = 0;
        command("ID"); command("SET 1 131073");
        require(sockets[owner].tx.contains("AD5780"), "Full USB debug buffer blocked network command");
        press(A2); advanceButtons(SAFE_STEP_US);
        require(dacCode(0)==ZERO_CODE, "Full USB debug buffer blocked safe button");
    } else if (id == "B12") {
        seedCodes(ZERO_CODE+1,ZERO_CODE,ZERO_CODE,ZERO_CODE);
        fakeNetworkMissing = true; press(A1);
        require(networkFault && !networkEnabled && SPI.writes==0, "Failed network reset touched DAC or accepted commands");
        press(A2); advanceButtons(SAFE_STEP_US);
        require(safeZeroLatched && dacCode(0)==ZERO_CODE, "Safe button failed after unsuccessful network reset");
    } else if (id == "B13") {
        DacNetworkWait wait;
        bool stopped = false;
        for (int i=0; i<100001; ++i) if(wait.expired()) { stopped=true; break; }
        require(stopped && networkFault, "Driver wait has no iteration fallback when clock does not advance");
    } else if (id == "B14") {
        require(gpioModes[SD_CS] == OUTPUT && gpioOutputs[SD_CS] == HIGH,
                "Uno R4 pinMode left SD selected on shared SPI bus");
        seedCodes(157286,ZERO_CODE,ZERO_CODE,ZERO_CODE); attachOwner();
        command("READ 1");
        require(sockets[owner].tx.contains("157286\r\n"), "Startup corrupted DAC readback");
        press(A1);
        require(gpioOutputs[SD_CS] == HIGH && SPI.writes == 0,
                "Network reset selected SD or wrote a DAC");
    } else if (id == "B15" || id == "B16") {
        digitalWrite(SD_CS, HIGH); // Isolate validation from startup-order test.
        seedCodes(157286,ZERO_CODE,ZERO_CODE,ZERO_CODE); attachOwner();
        for (uint32_t raw : {0xFFFFFFu, 0u, 0x2FFFFCu}) {
            SPI.forcedRead = raw; sockets[owner].tx.clear();
            command(id == "B15" ? "READ 1" : "RAMP 1 131072 1 1000");
            require(sockets[owner].tx.contains("ERR!") && !sockets[owner].tx.contains("262143\r\n"),
                    "Invalid readback returned voltage/success instead of error");
            require(SPI.writes == 0 && !dacBusy, "Invalid readback caused output write or leaked lock");
        }
    } else if (id == "B17") {
        digitalWrite(SD_CS, HIGH);
        seedCodes(157286,ZERO_CODE,ZERO_CODE,ZERO_CODE);
        // Failure on the last channel must be detected before ANY output write.
        SPI.forcedRead = 0xFFFFFF; SPI.forcedReadPin = DAC_sync[3];
        press(A2); advanceButtons(SAFE_STEP_US);
        require(SPI.writes == 0 && !safeZeroActive && safeZeroLatched && !dacBusy,
                "Safe ramp used an invalid starting value or failed to latch");
    } else if (id == "B18") {
        digitalWrite(SD_CS, HIGH); attachOwner();
        // Full scale is valid; R/W and low two don't-care bits are not data.
        for (uint32_t raw : {0x1FFFFCu, 0x9FFFFFu}) {
            SPI.forcedRead = raw; sockets[owner].tx.clear(); command("READ 1");
            require(sockets[owner].tx.contains("262143\r\n") && !sockets[owner].tx.contains("ERR!"),
                    "Valid full-scale readback was rejected");
        }
        SPI.forcedRead = 0x100000; sockets[owner].tx.clear(); command("READ 1");
        require(sockets[owner].tx.contains("0\r\n"), "Valid negative full scale rejected");
    } else if (id == "B19") {
        attachOwner();
        for (int direction : {1, -1}) for (uint32_t step : {1u, 10u, 100u}) {
            const uint32_t span = 1003, start = ZERO_CODE;
            const uint32_t target = direction > 0 ? start + span : start - span;
            const uint32_t count = (span + step - 1) / step;
            seedCodes(start,ZERO_CODE,ZERO_CODE,ZERO_CODE);
            SPI.writes = 0; delayCalls = 0;
            uint64_t before = fakeMicros;
            char cmd[80]; std::snprintf(cmd,sizeof(cmd),"RAMP 1 %u %u 1000",target,step);
            command(cmd);
            require(SPI.writes == int(count), "step_size did not control DAC write count");
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t distance = (i+1)*step; if(distance > span) distance=span;
                require(SPI.codes[i] == (direction > 0 ? start+distance : start-distance),
                        "Ramp did not use requested step_size or clamp the last step");
            }
            require(fakeMicros-before == 10+uint64_t(count)*1010,
                    "Requested ramp delays were skipped or repeated");
            require(dacCode(0)==target && !dacBusy, "Ramp failed to finish at target");
            std::cout << "direction=" << direction << " step=" << step
                      << " writes=" << count << " simulated_delay_us=" << fakeMicros-before << "\n";
        }
    } else if (id == "B20") {
        attachOwner();
        for (uint32_t waitUs : {10u, 100u, 1000u, 10000u}) {
            seedCodes(ZERO_CODE,ZERO_CODE,ZERO_CODE,ZERO_CODE);
            SPI.writes = 0; delayCalls = 0;
            uint64_t before = fakeMicros;
            char cmd[80]; std::snprintf(cmd,sizeof(cmd),"RAMP 1 %u 10 %u",ZERO_CODE+1000,waitUs);
            command(cmd);
            require(SPI.writes == 100 && fakeMicros-before == 10+100ULL*(waitUs+10),
                    "STEP_TIME is not applied as microseconds per DAC step");
            std::cout << "step=10 wait_us=" << waitUs << " writes=" << SPI.writes
                      << " simulated_delay_us=" << fakeMicros-before << "\n";
        }
        SPI.writes = 0; delayCalls = 0;
        command("RAMP 1 132072 1 1000000"); // Already at this target.
        require(SPI.writes == 0, "Repeated same-target ramp moved the DAC");
    } else throw std::runtime_error("Unknown button test");
}
