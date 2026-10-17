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

// Selected together with the physical unit identity from ignored local config.
// Stored in firmware so physical safe-zero remains independent of the PC.
DacCalibration channelCalibration[4] = DAC_CHANNEL_CALIBRATIONS;
