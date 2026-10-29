// SPDX-License-Identifier: MIT
#pragma once
#include <cstdio>

extern int g_failures;

#define CHECK(cond, msg) do {                               \
    if (!(cond)) { std::printf("  FAIL: %s\n", (msg)); g_failures++; } \
    else         { std::printf("  pass: %s\n", (msg)); }    \
} while (0)
