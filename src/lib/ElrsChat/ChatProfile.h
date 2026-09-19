#pragma once
#include "ChatEngine.h"

namespace chat {
// Hardware capability limits, not regional RF approval. The adapter separately
// enforces its experimental-RF gate and board-specific calibration limits.
inline bool selectSupportedProfile(Profile &p, uint8_t minPower, uint8_t maxPower,
                                   bool subGhz, bool band2400) {
    if (p.power > 7 || p.power < minPower || p.power > maxPower) return false;
    if (band2400 && p.frequency >= 2401000000UL && p.frequency <= 2482000000UL) {
        p.id = 1; return true;
    }
    if (subGhz && p.frequency >= 863000000UL && p.frequency <= 928000000UL) {
        p.id = 2; return true;
    }
    return false;
}
}
