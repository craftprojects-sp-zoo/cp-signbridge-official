// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — licznik zajętych miejsc (np. równoległych żądań we wtyczce), bez blokad
#pragma once
#include <windows.h>

namespace sb {

class Slots {
public:
    explicit Slots(LONG max) : max_(max), used_(0) {}
    bool TryAcquire() {
        if (InterlockedIncrement(&used_) > max_) { InterlockedDecrement(&used_); return false; }
        return true;
    }
    void Release() { InterlockedDecrement(&used_); }
    LONG Used() const { return used_; }
private:
    const LONG max_;
    volatile LONG used_;
};

}  // namespace sb
