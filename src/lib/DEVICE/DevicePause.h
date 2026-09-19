// SPDX-License-Identifier: GPL-3.0-or-later
// ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
#pragma once
#include <stdint.h>

namespace elrs {
// The owner core publishes a mask. The other core acknowledges it only at a
// callback-batch boundary, after its previous callbacks have completed.
// Normal operation requires one lock-free load, not a blocking mutex.
class DevicePause {
public:
    uint32_t request(uint32_t mask) {
        const uint32_t ticket = __atomic_load_n(&requested_, __ATOMIC_RELAXED) + 1;
        __atomic_store_n(&mask_, mask, __ATOMIC_RELAXED);
        __atomic_store_n(&requested_, ticket, __ATOMIC_RELEASE);
        return ticket;
    }
    void boundary(unsigned core) {
        const uint32_t ticket = __atomic_load_n(&requested_, __ATOMIC_ACQUIRE);
        if (localTicket_[core] == ticket) return;
        const uint32_t mask = __atomic_load_n(&mask_, __ATOMIC_RELAXED);
        if (__atomic_load_n(&requested_, __ATOMIC_ACQUIRE) != ticket) return;
        local_[core] = mask; localTicket_[core] = ticket;
        if (core == 0) __atomic_store_n(&acknowledged_, ticket, __ATOMIC_RELEASE);
    }
    bool acknowledged(uint32_t ticket) const {
        return __atomic_load_n(&acknowledged_, __ATOMIC_ACQUIRE) == ticket;
    }
    bool paused(unsigned core, unsigned device) const { return local_[core] & (uint32_t(1) << device); }
private:
    uint32_t requested_ = 0, acknowledged_ = 0, mask_ = 0;
    uint32_t local_[2] = {}, localTicket_[2] = {};
};
}
