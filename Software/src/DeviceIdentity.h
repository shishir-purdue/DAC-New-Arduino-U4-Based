#pragma once

// Real identities are generated into the ignored build directory from local
// configuration. A production build without that configuration must fail.
#ifdef DAC_HOST_TEST
#include "DeviceConfig.fixture.h"
#else
#include "DeviceConfig.generated.h"
#endif

#define DAC_MAC_BYTES DAC_CONFIGURED_MAC
const IPAddress deviceIP DAC_CONFIGURED_IP;
constexpr const char* networkReadyMessage = DAC_NETWORK_READY_MESSAGE;
