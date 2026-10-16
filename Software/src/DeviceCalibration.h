#pragma once
#include <stdint.h>

// V(code) = offsetNanovolts*1e-9 + code*stepPicovolts*1e-12.
// Integer protocol fields retain more precision than required by the meter.
struct DacCalibration {
    int64_t offsetNanovolts;
    uint32_t stepPicovolts;
    uint32_t zeroCode;
    bool measured;
};
constexpr DacCalibration nominalCalibration = {-10000000000LL, 76293945, 131072, false};

// The Uno R4 nano printf does not implement %lld. Format explicitly so host
// printf support cannot hide a broken on-device calibration response.
inline void calibrationIntegerText(int64_t value, char* output) {
    uint64_t magnitude = value < 0 ? uint64_t(-(value+1))+1 : uint64_t(value);
    char reversed[20]; unsigned length=0;
    do { reversed[length++]=char('0'+magnitude%10); magnitude/=10; } while(magnitude);
    if(value<0) *output++='-';
    while(length) *output++=reversed[--length];
    *output='\0';
}

// This project's MAC/IP identify box A8:61:0A:AE:F2:01. Only CH1 was measured.
// Source: 20261016T190817Z, Keysight 34461A MY60041761; fit residual <=11.94 uV.
// Build-time storage makes the local safe-zero endpoint independent of a PC,
// connection, reset, or flash-emulated EEPROM write. Do not reuse on another box.
#if defined(DAC_CALIBRATED_BOX) && DAC_CALIBRATED_BOX
DacCalibration channelCalibration[4] = {
    {-9992845256LL, 76283553, 130996, true},
    nominalCalibration, nominalCalibration, nominalCalibration
};
#else
DacCalibration channelCalibration[4] = {
    nominalCalibration, nominalCalibration, nominalCalibration, nominalCalibration
};
#endif
