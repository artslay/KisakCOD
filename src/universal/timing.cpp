#include <universal/q_shared.h>
#include "timing.h"

#include <chrono>

long double msecPerRawTimerTick;
double qpc2msec;

uint64_t Kisak_RawTimerTicks()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

double __cdecl SecondsPerTick()
{
    return 1.0e-9;
}

void __cdecl InitTiming()
{
    msecPerRawTimerTick = 1.0e-6L;
    qpc2msec = 1.0e-6;
}
