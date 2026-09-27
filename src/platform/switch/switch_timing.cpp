#ifdef __SWITCH__
#include "timing.h"
#include <chrono>
#include <thread>
#include <qcommon/threads.h>

long double msecPerRawTimerTick = 1.0L;
double qpc2msec = 1.0;

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
