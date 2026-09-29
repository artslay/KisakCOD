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

    char number[16];
    unsigned int value = BUILD_NUMBER;
    unsigned int pos = sizeof(number) - 1;
    number[pos] = '\0';

    do
    {
        number[--pos] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value);

    std::memcpy(buildnumbuf, &number[pos], sizeof(number) - pos);
    std::strcpy(&buildnumbuf[sizeof(number) - pos - 1], " ");
    std::strcat(buildnumbuf, __DATE__);
    std::strcat(buildnumbuf, " ");
    std::strcat(buildnumbuf, __TIME__);
	return buildnumbuf;
}

int getBuildNumberAsInt()
{
	return BUILD_NUMBER;
}