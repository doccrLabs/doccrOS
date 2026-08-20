/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: math.c
 * CREDITS: Offihito
 *
 */

#include <math.h>

/* Compact approximation used only to construct Doom's view-angle table. */
static double atan_unit(double x)
{
    return x * (0.7853981633974483 + 0.273 * (1.0 - x));
}

double atan(double x)
{
    int negative = x < 0.0;
    if (negative) x = -x;
    double result = x > 1.0
        ? 1.5707963267948966 - atan_unit(1.0 / x)
        : atan_unit(x)
    ;

    return negative ? -result : result;
}

double fabs(double x)
{
    return x < 0.0 ? -x : x;
}

double sqrt(double x)
{
    if (x < 0) return 0;
    double res;
    __asm__ ("sqrtsd %1, %0" : "=x"(res) : "x"(x));
    return res;
}

double sin(double x)
{
    double res;
    __asm__ ("fsin" : "=t"(res) : "0"(x));
    return res;
}

double cos(double x)
{
    double res;
    __asm__ ("fcos" : "=t"(res) : "0"(x));
    return res;
}

double pow(double x, double y)
{
    double res;
    // x^y = 2^(y * log2(x))
    // This is a simplified x87 implementation
    __asm__ (
        "fyl2x;"            // st(1) = y * log2(x), pop st(0)
        "fld %%st(0);"      // duplicate y*log2(x)
        "frndint;"          // round to integer
        "fsubr %%st(1), %%st(0);" // get fractional part
        "f2xm1;"            // 2^(fractional) - 1
        "fld1;"
        "faddp;"             // 2^(fractional)
        "fscale;"            // st(0) * 2^st(1)
        "fstp %%st(1);"      // pop integer part
        : "=t"(res)
        : "0"(x), "u"(y)
    );
    return res;
}

double floor(double x)
{
    if (x >= 0)
    {
        return (double)(long)x;
    } else
    {
        long i = (long)x;
        if (x == (double)i) return x;
        return (double)(i - 1);
    }
}