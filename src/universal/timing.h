#pragma once


void InitTiming();

extern long double msecPerRawTimerTick;
extern double qpc2msec;

#include <cstdint>

uint64_t Kisak_RawTimerTicks();
