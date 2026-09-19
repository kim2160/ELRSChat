// SPDX-License-Identifier: GPL-3.0-or-later
// ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
#pragma once
#include "targets.h"
#if defined(ELRS_CHAT) && defined(TARGET_TX)
#include "crsf_protocol.h"
#include "ChatProtocol.h"
struct stringParameter;
extern volatile bool chatRcPaused;
extern bool chatHandsetPresent;
extern bool chatHandsetNeedsModel;
extern bool chatDeviceScheduled;
extern stringParameter chatParameter;
inline bool ICACHE_RAM_ATTR chatDeviceActive() { return chatRcPaused; }
chat::Result chatRcSuspend();
void chatRcRestore(bool resume);
void chatDeviceBegin(bool radioAvailable);
void chatDeviceLoop(uint32_t now);
bool chatDeviceHandleMessage(const crsf_header_t *frame);
void chatDeviceDisconnected();
#endif
