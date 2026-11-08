#pragma once
#include "acquisition.hpp"
namespace dmotor_monitor {
// Tick wakeups are hints, not proof that an esp_timer deadline has arrived.
// Preserve the absolute schedule; never rebase it onto an early/late wakeup.
// pause may return early. A backwards clock aborts rather than authorizing work.
template<class Pause>
bool wait_for_release(Clock& clock, Us release, Pause pause) {
    if(release<0) return false;
    Us now=clock.now();
    while(now<release) {
        if(now<0) return false;
        pause(release-now);
        const Us next=clock.now();
        if(next<now) return false;
        now=next;
    }
    return true;
}
} // namespace dmotor_monitor
