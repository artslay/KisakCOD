#ifdef __SWITCH__
#include <universal/timing.h>
#include <chrono>
#include <thread>
#include <qcommon/threads.h>

long double msecPerRawTimerTick = 1.0L;
double qpc2msec = 1.0;

uint64_t Kisak_RawTimerTicks()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

double __cdecl SecondsPerTick()
{
    return 1.0e-3;
}

void __cdecl InitTiming()
{
    msecPerRawTimerTick = 1.0L;
    qpc2msec = 1.0;
}
#endif
