#include <universal/q_shared.h>
#include "buildnumber.h"
#include <stdio.h>

char buildnumbuf[128];

/*
* Original Date: "Thu Oct 04 00:43:04 2007"
* Original Build: 13620
*/

// LWSS: shared between SP/MP for simplicity

char *__cdecl getBuildNumber()
{
#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

    char digits[16];
    unsigned int value = BUILD_NUMBER;
    int digitCount = 0;
    char *out = buildnumbuf;

    do
    {
        digits[digitCount++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value);

    while (digitCount > 0)
        *out++ = digits[--digitCount];

    *out++ = ' ';

    const char *date = __DATE__;
    while (*date)
        *out++ = *date++;

    *out++ = ' ';

    const char *buildTime = __TIME__;
    while (*buildTime)
        *out++ = *buildTime++;

    *out = '\0';
	return buildnumbuf;
}

int getBuildNumberAsInt()
{
	return BUILD_NUMBER;
}