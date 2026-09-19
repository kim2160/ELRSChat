// SPDX-License-Identifier: GPL-3.0-or-later
// ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
#pragma once
#include "ChatProtocol.h"

namespace chat {
class SessionPort {
public:
    virtual ~SessionPort() {}
    // Failure must leave ordinary RC running. Success leaves its timer stopped.
    virtual Result suspendRc() = 0;
    virtual bool configureChat() = 0;
    virtual void stopChat() = 0;
    virtual void restoreRc(bool resume) = 0;
};
// Shared by the real adapter and native tests. Configuration is temporary and
// restoration occurs once, including failed entry and lost Lua sessions.
class Session {
public:
    explicit Session(SessionPort &port) : port_(port) {}
    Result enter() {
        if (active_) return Result::None;
        const Result result = port_.suspendRc();
        if (result != Result::None) return result;
        active_ = true;
        if (!port_.configureChat()) { leave(); return Result::Unsupported; }
        return Result::None;
    }
    void leave(bool resume = true) {
        if (!active_) return;
        port_.stopChat();
        port_.restoreRc(resume);
        active_ = false;
    }
    bool active() const { return active_; }
private:
    SessionPort &port_;
    bool active_ = false;
};
}
