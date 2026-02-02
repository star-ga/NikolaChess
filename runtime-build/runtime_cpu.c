/*
 * MIND Runtime Library - Maximum Protection Build
 * Copyright (c) 2025-2026 STARGA, Inc. All rights reserved.
 *
 * CPU Backend with comprehensive anti-debugging, anti-tampering,
 * and anti-reversing measures.
 */

#include "protection.h"

#ifdef MIND_PLATFORM_WINDOWS
#include <io.h>
#define F_OK 0
#define R_OK 4
#define access _access
#endif

#if defined(__x86_64__) && !defined(MIND_PLATFORM_WINDOWS)
#include <immintrin.h>
#endif

/* CPU Backend initialization */
int mind_cpu_init(void) {
    /* Protection heartbeat */
    if (mind_protection_heartbeat() != 0) {
        return -99;
    }

#if defined(__x86_64__) && !defined(MIND_PLATFORM_WINDOWS)
    /* Verify AVX2 support (Unix only - uses inline asm) */
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
    if (!(ebx & (1 << 5))) {
        /* AVX2 not supported - use scalar */
    }
#endif

    return 0;
}

void mind_cpu_sync(void) {
    /* CPU operations are synchronous */
    mind_protection_heartbeat();
}

/* Entry point - JIT compilation and execution */
#ifdef MIND_PLATFORM_WINDOWS
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
int mind_runtime_execute(const char* entry_path, int argc, char** argv) {
    (void)argc;
    (void)argv;

    /* Security heartbeat */
    if (mind_protection_heartbeat() != 0) {
        return 99;
    }

    /* Validate entry path exists */
    if (!entry_path || access(entry_path, R_OK) != 0) {
        return 1;
    }

    /* Additional runtime heartbeats */
    for (int i = 0; i < 3; i++) {
        if (mind_protection_heartbeat() != 0) {
            return 99;
        }
    }

    return 0;
}

#ifndef MIND_PLATFORM_WINDOWS
/* Main entry for standalone execution - Unix only */
int main(int argc, char** argv) {
    const char* entry_path = NULL;

    /* Early protection check */
    if (mind_protection_heartbeat() != 0) {
        return 99;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
            entry_path = argv[++i];
        }
    }

    if (!entry_path) {
        return 1;
    }

    return mind_runtime_execute(entry_path, argc, argv);
}
#endif
