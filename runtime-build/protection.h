/*
 * MIND Runtime Protection Suite v2.0
 * Copyright (c) 2025-2026 STARGA, Inc. All rights reserved.
 *
 * MAXIMUM local protection - all known anti-debugging, anti-tampering,
 * anti-reversing, and anti-analysis techniques implemented.
 * Supports Linux (x64/arm64), macOS (x64/arm64), and Windows (x64).
 */

#ifndef MIND_PROTECTION_H
#define MIND_PROTECTION_H

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define MIND_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define MIND_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define MIND_PLATFORM_LINUX 1
#endif

/* Must be before any includes on Unix */
#if !defined(MIND_PLATFORM_WINDOWS)
    #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef MIND_PLATFORM_WINDOWS
/* Windows includes */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <intrin.h>
#pragma comment(lib, "ntdll.lib")

/* Windows types */
typedef HANDLE pthread_t;
typedef int sig_atomic_t;
#define sigjmp_buf jmp_buf
#define sigsetjmp(env, save) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)

#else
/* Unix includes */
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#endif

#ifdef MIND_PLATFORM_LINUX
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/prctl.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/personality.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sched.h>
#include <pthread.h>
#endif

#ifdef MIND_PLATFORM_MACOS
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#endif

/* Obfuscated strings - XOR with key 0x5A */
static const unsigned char _s_tracer[] = {0x0e, 0x38, 0x3f, 0x3d, 0x3b, 0x38, 0x0a, 0x3f, 0x3e, 0x7a, 0x00}; /* TracerPid: */
static const unsigned char _s_status[] = {0x75, 0x36, 0x38, 0x3b, 0x3d, 0x75, 0x39, 0x3b, 0x3c, 0x30, 0x75, 0x39, 0x2e, 0x3f, 0x2e, 0x2f, 0x39, 0x00}; /* /proc/self/status */
static const unsigned char _s_maps[] = {0x75, 0x36, 0x38, 0x3b, 0x3d, 0x75, 0x39, 0x3b, 0x3c, 0x30, 0x75, 0x33, 0x3f, 0x36, 0x39, 0x00}; /* /proc/self/maps */
static const unsigned char _s_proc[] = {0x75, 0x36, 0x38, 0x3b, 0x3d, 0x00}; /* /proc */
static const unsigned char _s_gdb[] = {0x3d, 0x3e, 0x3a, 0x00}; /* gdb */
static const unsigned char _s_lldb[] = {0x36, 0x36, 0x3e, 0x3a, 0x00}; /* lldb */
static const unsigned char _s_strace[] = {0x39, 0x2e, 0x38, 0x3f, 0x3d, 0x3b, 0x00}; /* strace */
static const unsigned char _s_ltrace[] = {0x36, 0x2e, 0x38, 0x3f, 0x3d, 0x3b, 0x00}; /* ltrace */
static const unsigned char _s_ida[] = {0x33, 0x3e, 0x3f, 0x00}; /* ida */
static const unsigned char _s_ghidra[] = {0x3d, 0x32, 0x33, 0x3e, 0x38, 0x3f, 0x00}; /* ghidra */
static const unsigned char _s_radare[] = {0x38, 0x3f, 0x3e, 0x3f, 0x38, 0x3b, 0x00}; /* radare */
static const unsigned char _s_frida[] = {0x30, 0x38, 0x33, 0x3e, 0x3f, 0x00}; /* frida */
static const unsigned char _s_ld_preload[] = {0x16, 0x1e, 0x7f, 0x0a, 0x08, 0x1b, 0x16, 0x19, 0x1f, 0x1e, 0x00}; /* LD_PRELOAD */
static const unsigned char _s_libasan[] = {0x36, 0x33, 0x3a, 0x3f, 0x39, 0x3f, 0x34, 0x00}; /* libasan */
static const unsigned char _s_vbox[] = {0x2c, 0x3a, 0x3b, 0x28, 0x00}; /* VBox */
static const unsigned char _s_vmware[] = {0x0c, 0x13, 0x21, 0x3f, 0x38, 0x3b, 0x00}; /* VMware */
static const unsigned char _s_qemu[] = {0x0b, 0x1b, 0x13, 0x0f, 0x00}; /* QEMU */
/* Additional obfuscated strings for enhanced protection */
static const unsigned char _s_ld_audit[] = {0x16, 0x1e, 0x7f, 0x1f, 0x0f, 0x1e, 0x13, 0x0e, 0x00}; /* LD_AUDIT */
static const unsigned char _s_ld_debug[] = {0x16, 0x1e, 0x7f, 0x1e, 0x1b, 0x1a, 0x0f, 0x1d, 0x00}; /* LD_DEBUG */
static const unsigned char _s_dyld_insert[] = {0x1e, 0x03, 0x16, 0x1e, 0x7f, 0x13, 0x18, 0x09, 0x1b, 0x08, 0x0e, 0x7f, 0x16, 0x13, 0x1a, 0x08, 0x1f, 0x08, 0x13, 0x1b, 0x09, 0x00}; /* DYLD_INSERT_LIBRARIES */
static const unsigned char _s_nikola[] = {0x34, 0x33, 0x35, 0x3b, 0x36, 0x3f, 0x00}; /* nikola */
static const unsigned char _s_objection[] = {0x3b, 0x3a, 0x32, 0x3b, 0x3d, 0x2e, 0x33, 0x3b, 0x34, 0x00}; /* objection */
static const unsigned char _s_cycript[] = {0x39, 0x23, 0x3d, 0x38, 0x33, 0x36, 0x2e, 0x00}; /* cycript */
static const unsigned char _s_substrate[] = {0x39, 0x2f, 0x3a, 0x39, 0x2e, 0x38, 0x3f, 0x2e, 0x3b, 0x00}; /* substrate */
static const unsigned char _s_xcode[] = {0x28, 0x3d, 0x3b, 0x3e, 0x3b, 0x00}; /* Xcode */
static const unsigned char _s_instruments[] = {0x13, 0x34, 0x39, 0x2e, 0x38, 0x2f, 0x33, 0x3b, 0x34, 0x2e, 0x39, 0x00}; /* Instruments */
static const unsigned char _s_dtrace[] = {0x3e, 0x2e, 0x38, 0x3f, 0x3d, 0x3b, 0x00}; /* dtrace */
static const unsigned char _s_hopper[] = {0x32, 0x3b, 0x36, 0x36, 0x3b, 0x38, 0x00}; /* hopper */
static const unsigned char _s_cutter[] = {0x39, 0x2f, 0x2e, 0x2e, 0x3b, 0x38, 0x00}; /* cutter */
static const unsigned char _s_x64dbg[] = {0x28, 0x6e, 0x6c, 0x3e, 0x3a, 0x3d, 0x00}; /* x64dbg */
static const unsigned char _s_ollydbg[] = {0x3b, 0x36, 0x36, 0x23, 0x3e, 0x3a, 0x3d, 0x00}; /* ollydbg */
static const unsigned char _s_windbg[] = {0x21, 0x33, 0x34, 0x3e, 0x3a, 0x3d, 0x00}; /* windbg */
static const unsigned char _s_r2[] = {0x38, 0x68, 0x00}; /* r2 */

/* Decode obfuscated string */
static inline void _decode_str(const unsigned char *src, char *dst, size_t len) {
    for (size_t i = 0; i < len && src[i]; i++) {
        dst[i] = src[i] ^ 0x5A;
        dst[i+1] = '\0';
    }
}

/* Protection state */
static volatile int _protection_initialized = 0;
static volatile uint64_t _code_checksum = 0;
static volatile uint64_t _last_heartbeat = 0;
static volatile uint64_t _stack_canary = 0;
static volatile void* _text_base = NULL;
static volatile size_t _text_size = 0;
static volatile int _monitor_running = 0;

/* Polymorphic key - changes at runtime */
static volatile uint32_t _poly_key = 0x5A5A5A5A;

/* Platform-specific state */
#ifdef MIND_PLATFORM_WINDOWS
static HANDLE _monitor_thread = NULL;
static volatile LONG _trap_triggered = 0;
#else
static sigjmp_buf _trap_jmp;
static volatile sig_atomic_t _trap_triggered = 0;
static pthread_t _monitor_thread = 0;
#endif

/* Compiler-specific attributes */
#ifdef MIND_PLATFORM_WINDOWS
#define MIND_NOINLINE __declspec(noinline)
#define MIND_HIDDEN
#define MIND_CONSTRUCTOR
#else
#define MIND_NOINLINE __attribute__((noinline))
#define MIND_HIDDEN __attribute__((visibility("hidden")))
#define MIND_CONSTRUCTOR __attribute__((constructor))
#endif

/*
 * LAYER 1: Anti-Debugging
 */

/* 1a. TracerPid check */
static int _check_tracer_pid(void) {
#ifdef __linux__
    char path[64], line[256];
    _decode_str(_s_status, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (f) {
        char tracer[16];
        _decode_str(_s_tracer, tracer, sizeof(tracer));

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, tracer, 10) == 0) {
                int pid = atoi(line + 10);
                fclose(f);
                return pid != 0;
            }
        }
        fclose(f);
    }
#endif

#ifdef __APPLE__
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc info;
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, NULL, 0) == 0) {
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
#endif
    return 0;
}

/* 1b. PTRACE self-attach (prevents other debuggers) */
static int _check_ptrace_attach(void) {
#ifdef __linux__
    /* Try to trace ourselves - if another debugger is attached, this fails */
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
        return 1; /* Already being traced */
    }
    /* Detach immediately */
    ptrace(PTRACE_DETACH, 0, NULL, NULL);
#endif
    return 0;
}

/* 1c. Hardware breakpoint detection */
static int _check_hardware_breakpoints(void) {
#if defined(__linux__) && defined(__x86_64__)
    unsigned long dr0, dr1, dr2, dr3, dr7;

    /* Read debug registers via ptrace of child */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: allow parent to trace us */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        raise(SIGSTOP);
        _exit(0);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);

        /* Read debug registers */
        dr0 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[0]), NULL);
        dr1 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[1]), NULL);
        dr2 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[2]), NULL);
        dr3 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[3]), NULL);
        dr7 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[7]), NULL);

        ptrace(PTRACE_CONT, pid, NULL, NULL);
        waitpid(pid, &status, 0);

        /* Check if any hardware breakpoints are set */
        if (dr0 || dr1 || dr2 || dr3 || (dr7 & 0xFF)) {
            return 1;
        }
    }
#endif
    return 0;
}

/* 1d. SIGTRAP handler for breakpoint detection */
static void _sigtrap_handler(int sig) {
    (void)sig;
    _trap_triggered = 1;
    siglongjmp(_trap_jmp, 1);
}

static int _check_breakpoint_trap(void) {
#if defined(__linux__) || defined(__APPLE__)
    struct sigaction sa, old_sa;
    sa.sa_handler = _sigtrap_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTRAP, &sa, &old_sa);

    _trap_triggered = 0;
    if (sigsetjmp(_trap_jmp, 1) == 0) {
        /* Trigger SIGTRAP - if debugger intercepts, we won't reach handler */
        #if defined(__x86_64__)
        __asm__ volatile("int $3");
        #elif defined(__aarch64__)
        __asm__ volatile("brk #0");
        #endif
    }

    sigaction(SIGTRAP, &old_sa, NULL);

    /* If trap wasn't triggered, debugger intercepted it */
    return !_trap_triggered;
#else
    return 0;
#endif
}

/* 1e. Timing-based detection with jitter */
static int _check_timing_anomaly(void) {
#if defined(__linux__) || defined(__APPLE__)
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Multiple timing samples to detect inconsistent stepping */
    volatile uint64_t x = 0;
    for (int j = 0; j < 3; j++) {
        for (volatile int i = 0; i < 5000; i++) {
            x += i * i;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t elapsed = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                       (end.tv_nsec - start.tv_nsec);

    /* More than 100ms for simple loop = stepping */
    return elapsed > 100000000;
#else
    return 0;
#endif
}

/* 1f. Parent process validation */
static int _check_parent_process(void) {
#ifdef __linux__
    char path[64], cmdline[256];
    pid_t ppid = getppid();

    snprintf(path, sizeof(path), "/proc/%d/cmdline", ppid);

    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        ssize_t len = read(fd, cmdline, sizeof(cmdline) - 1);
        close(fd);

        if (len > 0) {
            cmdline[len] = '\0';

            /* Check for known debuggers in parent */
            char gdb[8], lldb[8], strace[8], ltrace[8], ida[8], radare[8];
            _decode_str(_s_gdb, gdb, sizeof(gdb));
            _decode_str(_s_lldb, lldb, sizeof(lldb));
            _decode_str(_s_strace, strace, sizeof(strace));
            _decode_str(_s_ltrace, ltrace, sizeof(ltrace));
            _decode_str(_s_ida, ida, sizeof(ida));
            _decode_str(_s_radare, radare, sizeof(radare));

            if (strstr(cmdline, gdb) || strstr(cmdline, lldb) ||
                strstr(cmdline, strace) || strstr(cmdline, ltrace) ||
                strstr(cmdline, ida) || strstr(cmdline, radare)) {
                return 1;
            }
        }
    }
#endif
    return 0;
}

/*
 * LAYER 2: Environment Checks
 */

/* 2a. LD_PRELOAD detection */
static int _check_ld_preload(void) {
#ifdef __linux__
    char env_name[16];
    _decode_str(_s_ld_preload, env_name, sizeof(env_name));

    if (getenv(env_name) != NULL) {
        return 1;
    }

    /* Also check /proc/self/environ */
    char path[32];
    _decode_str(_s_status, path, sizeof(path));
    path[11] = 'e'; path[12] = 'n'; path[13] = 'v';
    path[14] = 'i'; path[15] = 'r'; path[16] = 'o';
    path[17] = 'n'; path[18] = '\0';

    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buf[4096];
        ssize_t len = read(fd, buf, sizeof(buf));
        close(fd);

        if (len > 0 && memmem(buf, len, env_name, strlen(env_name))) {
            return 1;
        }
    }
#endif
    return 0;
}

/* 2b. Debugger library detection in memory maps */
static int _check_debugger_libs(void) {
#ifdef __linux__
    char path[32], line[512];
    _decode_str(_s_maps, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (f) {
        char gdb[8], frida[8], libasan[12];
        _decode_str(_s_gdb, gdb, sizeof(gdb));
        _decode_str(_s_frida, frida, sizeof(frida));
        _decode_str(_s_libasan, libasan, sizeof(libasan));

        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, gdb) || strstr(line, frida) ||
                strstr(line, libasan) || strstr(line, "libdl-") ||
                strstr(line, "valgrind")) {
                fclose(f);
                return 1;
            }
        }
        fclose(f);
    }
#endif
    return 0;
}

/* 2c. Debugger process detection */
static int _check_debugger_processes(void) {
#ifdef __linux__
    DIR *proc;
    struct dirent *entry;
    char path[64];

    _decode_str(_s_proc, path, sizeof(path));

    proc = opendir(path);
    if (proc) {
        char gdb[8], lldb[8], strace[8], ltrace[8], ghidra[12], ida[8], radare[12], frida[8];
        _decode_str(_s_gdb, gdb, sizeof(gdb));
        _decode_str(_s_lldb, lldb, sizeof(lldb));
        _decode_str(_s_strace, strace, sizeof(strace));
        _decode_str(_s_ltrace, ltrace, sizeof(ltrace));
        _decode_str(_s_ghidra, ghidra, sizeof(ghidra));
        _decode_str(_s_ida, ida, sizeof(ida));
        _decode_str(_s_radare, radare, sizeof(radare));
        _decode_str(_s_frida, frida, sizeof(frida));

        while ((entry = readdir(proc)) != NULL) {
            /* Only check numeric directories (PIDs) */
            if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
                char cmdpath[80], cmdline[256];
                snprintf(cmdpath, sizeof(cmdpath), "/proc/%s/cmdline", entry->d_name);

                int fd = open(cmdpath, O_RDONLY);
                if (fd >= 0) {
                    ssize_t len = read(fd, cmdline, sizeof(cmdline) - 1);
                    close(fd);

                    if (len > 0) {
                        cmdline[len] = '\0';
                        if (strstr(cmdline, gdb) || strstr(cmdline, lldb) ||
                            strstr(cmdline, strace) || strstr(cmdline, ltrace) ||
                            strstr(cmdline, ghidra) || strstr(cmdline, ida) ||
                            strstr(cmdline, radare) || strstr(cmdline, frida)) {
                            closedir(proc);
                            return 1;
                        }
                    }
                }
            }
        }
        closedir(proc);
    }
#endif
    return 0;
}

/* 2d. VM detection */
static int _check_virtual_machine(void) {
#if defined(__x86_64__) || defined(__i386__)
    /* CPUID check for hypervisor */
    unsigned int eax, ebx, ecx, edx;

    /* Check hypervisor present bit */
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    if (ecx & (1 << 31)) {
        /* Hypervisor present - could be VM */
        /* Get hypervisor vendor */
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x40000000));

        char vendor[13];
        memcpy(vendor, &ebx, 4);
        memcpy(vendor + 4, &ecx, 4);
        memcpy(vendor + 8, &edx, 4);
        vendor[12] = '\0';

        /* Known VM vendors - allow some, block analysis VMs */
        if (strstr(vendor, "KVMKVMKVM") ||      /* KVM - often used for analysis */
            strstr(vendor, "VBoxVBoxVBox")) {   /* VirtualBox */
            return 1;
        }
    }

    /* Check for VM artifacts in DMI */
    #ifdef __linux__
    int fd = open("/sys/class/dmi/id/product_name", O_RDONLY);
    if (fd >= 0) {
        char name[64];
        ssize_t len = read(fd, name, sizeof(name) - 1);
        close(fd);

        if (len > 0) {
            name[len] = '\0';
            char vbox[8], vmware[12], qemu[8];
            _decode_str(_s_vbox, vbox, sizeof(vbox));
            _decode_str(_s_vmware, vmware, sizeof(vmware));
            _decode_str(_s_qemu, qemu, sizeof(qemu));

            if (strstr(name, vbox) || strstr(name, vmware) || strstr(name, qemu)) {
                return 1;
            }
        }
    }
    #endif
#endif
    return 0;
}

/* 2e. Execution context verification - ONLY works for NikolaChess */
static int _check_execution_context(void) {
#ifdef __linux__
    char exe[256];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';

        /* Convert to lowercase for case-insensitive check */
        for (char *p = exe; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p += 32;
        }

        /* STRICT: Must contain "nikola" or be in .nikolachess directory */
        int valid = 0;
        if (strstr(exe, "nikola") != NULL) valid = 1;
        if (strstr(exe, ".nikolachess") != NULL) valid = 1;

        if (!valid) {
            return 1; /* Unauthorized use */
        }
    }
#endif

#ifdef __APPLE__
    char exe[1024];
    uint32_t size = sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) == 0) {
        /* Convert to lowercase */
        for (char *p = exe; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p += 32;
        }

        /* STRICT: Must contain "nikola" or be in .nikolachess directory */
        int valid = 0;
        if (strstr(exe, "nikola") != NULL) valid = 1;
        if (strstr(exe, ".nikolachess") != NULL) valid = 1;

        if (!valid) {
            return 1; /* Unauthorized use */
        }
    }
#endif
    return 0;
}

/* 2f. LD_AUDIT/LD_DEBUG detection (Linux hooking vectors) */
static int _check_ld_audit_debug(void) {
#ifdef __linux__
    char ld_audit[16], ld_debug[16];
    _decode_str(_s_ld_audit, ld_audit, sizeof(ld_audit));
    _decode_str(_s_ld_debug, ld_debug, sizeof(ld_debug));

    if (getenv(ld_audit) != NULL) return 1;
    if (getenv(ld_debug) != NULL) return 1;

    /* Check /proc/self/environ for hidden vars */
    int fd = open("/proc/self/environ", O_RDONLY);
    if (fd >= 0) {
        char buf[8192];
        ssize_t len = read(fd, buf, sizeof(buf));
        close(fd);

        if (len > 0 && (memmem(buf, len, ld_audit, strlen(ld_audit)) ||
                        memmem(buf, len, ld_debug, strlen(ld_debug)))) {
            return 1;
        }
    }
#endif
    return 0;
}

/* 2g. DYLD_INSERT_LIBRARIES detection (macOS equivalent of LD_PRELOAD) */
static int _check_dyld_insert(void) {
#ifdef __APPLE__
    char dyld_insert[32];
    _decode_str(_s_dyld_insert, dyld_insert, sizeof(dyld_insert));

    if (getenv(dyld_insert) != NULL) return 1;

    /* Check for common injection frameworks */
    char substrate[16], cycript[12], objection[16], frida[8];
    _decode_str(_s_substrate, substrate, sizeof(substrate));
    _decode_str(_s_cycript, cycript, sizeof(cycript));
    _decode_str(_s_objection, objection, sizeof(objection));
    _decode_str(_s_frida, frida, sizeof(frida));

    /* Check loaded dylibs for injection frameworks */
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char *name = _dyld_get_image_name(i);
        if (name) {
            if (strstr(name, substrate) || strstr(name, cycript) ||
                strstr(name, objection) || strstr(name, frida)) {
                return 1;
            }
        }
    }
#endif
    return 0;
}

/* 2h. macOS exception port detection (debugger attachment via Mach) */
static int _check_exception_ports(void) {
#ifdef __APPLE__
    mach_port_t task = mach_task_self();
    mach_port_t exception_port;
    exception_mask_t mask = EXC_MASK_ALL;
    mach_msg_type_number_t count = 1;
    exception_mask_t masks[1];
    exception_handler_t handlers[1];
    exception_behavior_t behaviors[1];
    thread_state_flavor_t flavors[1];

    kern_return_t kr = task_get_exception_ports(
        task, mask, masks, &count, handlers, behaviors, flavors);

    if (kr == KERN_SUCCESS && count > 0 && handlers[0] != MACH_PORT_NULL) {
        /* Exception port is registered - likely debugger */
        return 1;
    }
#endif
    return 0;
}

/* 2i. macOS process info via task_info */
static int _check_task_info(void) {
#ifdef __APPLE__
    mach_port_t task = mach_task_self();
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;

    if (task_info(task, TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        /* Check suspend count - debuggers often suspend */
        if (info.suspend_count > 0) {
            return 1;
        }
    }
#endif
    return 0;
}

/* 2j. /proc/self/mem access detection (memory inspection) */
static int _check_mem_access(void) {
#ifdef __linux__
    /* Try to detect if someone has our memory mapped for reading */
    char buf[64];
    int fd = open("/proc/self/fdinfo/0", O_RDONLY);
    if (fd >= 0) {
        close(fd);
    }

    /* Check for unusual file descriptors pointing to our memory */
    DIR *fd_dir = opendir("/proc/self/fd");
    if (fd_dir) {
        struct dirent *entry;
        while ((entry = readdir(fd_dir)) != NULL) {
            if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
                char link_path[64], target[256];
                snprintf(link_path, sizeof(link_path), "/proc/self/fd/%s", entry->d_name);
                ssize_t len = readlink(link_path, target, sizeof(target) - 1);
                if (len > 0) {
                    target[len] = '\0';
                    /* Check for proc mem access */
                    if (strstr(target, "/proc/") && strstr(target, "/mem")) {
                        closedir(fd_dir);
                        return 1;
                    }
                }
            }
        }
        closedir(fd_dir);
    }
#endif
    return 0;
}

/* 2k. ASLR verification */
static int _check_aslr_disabled(void) {
#ifdef __linux__
    /* Check if personality is set to disable ASLR */
    int pers = personality(0xffffffff);
    if (pers != -1 && (pers & ADDR_NO_RANDOMIZE)) {
        return 1; /* ASLR disabled - suspicious */
    }

    /* Check /proc/sys/kernel/randomize_va_space */
    int fd = open("/proc/sys/kernel/randomize_va_space", O_RDONLY);
    if (fd >= 0) {
        char val;
        if (read(fd, &val, 1) == 1 && val == '0') {
            close(fd);
            return 1; /* ASLR disabled system-wide */
        }
        close(fd);
    }
#endif
    return 0;
}

/* 2l. Core dump detection/prevention */
static int _check_core_dumps(void) {
#if defined(__linux__) || defined(__APPLE__)
    struct rlimit rl;

    /* Disable core dumps */
    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rl);

    /* Verify they're disabled */
    getrlimit(RLIMIT_CORE, &rl);
    if (rl.rlim_cur != 0 || rl.rlim_max != 0) {
        return 1; /* Couldn't disable - someone overriding */
    }
#endif

#ifdef __linux__
    /* Also set prctl to prevent core dumps */
    prctl(PR_SET_DUMPABLE, 0);
    if (prctl(PR_GET_DUMPABLE) != 0) {
        return 1;
    }
#endif
    return 0;
}

/*
 * LAYER 3: Anti-Tampering
 */

/* 3a. Code section checksum */
static uint64_t _compute_code_checksum(void) {
#ifdef __linux__
    Dl_info info;
    if (dladdr((void*)_compute_code_checksum, &info)) {
        /* Get base address */
        void *base = info.dli_fbase;

        /* Read ELF header */
        Elf64_Ehdr *ehdr = (Elf64_Ehdr*)base;
        if (ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
            ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
            ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
            ehdr->e_ident[EI_MAG3] == ELFMAG3) {

            /* Find .text section and compute checksum */
            Elf64_Shdr *shdr = (Elf64_Shdr*)((char*)base + ehdr->e_shoff);
            char *strtab = (char*)base + shdr[ehdr->e_shstrndx].sh_offset;

            for (int i = 0; i < ehdr->e_shnum; i++) {
                if (strcmp(strtab + shdr[i].sh_name, ".text") == 0) {
                    /* FNV-1a hash of .text section */
                    uint64_t hash = 14695981039346656037ULL;
                    unsigned char *text = (unsigned char*)base + shdr[i].sh_offset;
                    size_t size = shdr[i].sh_size;

                    for (size_t j = 0; j < size; j++) {
                        hash ^= text[j];
                        hash *= 1099511628211ULL;
                    }
                    return hash;
                }
            }
        }
    }
#endif
    return 0;
}

static int _check_code_integrity(void) {
    if (_code_checksum == 0) return 0; /* Not initialized yet */

    uint64_t current = _compute_code_checksum();
    return current != _code_checksum;
}

/* 3b. GOT/PLT hook detection */
static int _check_got_hooks(void) {
#ifdef __linux__
    /* Check if libc functions point to expected locations */
    void *libc_printf = dlsym(RTLD_NEXT, "printf");
    void *libc_fopen = dlsym(RTLD_NEXT, "fopen");

    Dl_info info1, info2;
    if (dladdr(libc_printf, &info1) && dladdr(libc_fopen, &info2)) {
        /* Both should be in libc */
        if (strstr(info1.dli_fname, "libc") == NULL ||
            strstr(info2.dli_fname, "libc") == NULL) {
            return 1; /* Hooked! */
        }
    }
#endif
    return 0;
}

/*
 * LAYER 4: Memory Protection
 */

/* 4a. Stack canary verification */
static int _check_stack_canary(void) {
    if (_stack_canary == 0) return 0; /* Not initialized */

    volatile uint64_t current;
    #if defined(__x86_64__)
    __asm__ volatile("mov %%fs:0x28, %0" : "=r"(current));
    #elif defined(__aarch64__)
    /* ARM64 uses different mechanism */
    current = _stack_canary;
    #else
    current = _stack_canary;
    #endif

    /* Canary shouldn't change during execution */
    return 0; /* Can't easily verify without storing original */
}

/* 4b. Memory region permission monitoring */
static int _check_memory_permissions(void) {
#ifdef __linux__
    if (_text_base == NULL || _text_size == 0) return 0;

    char path[32], line[512];
    _decode_str(_s_maps, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (f) {
        uintptr_t text_addr = (uintptr_t)_text_base;

        while (fgets(line, sizeof(line), f)) {
            uintptr_t start, end;
            char perms[8];

            if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
                if (start <= text_addr && text_addr < end) {
                    /* .text should be r-x, not rwx (writable = patched) */
                    if (perms[0] == 'r' && perms[1] == 'w' && perms[2] == 'x') {
                        fclose(f);
                        return 1; /* Text section is writable - suspicious */
                    }
                    break;
                }
            }
        }
        fclose(f);
    }
#endif

#ifdef __APPLE__
    if (_text_base == NULL || _text_size == 0) return 0;

    mach_port_t task = mach_task_self();
    mach_vm_address_t addr = (mach_vm_address_t)_text_base;
    mach_vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    if (mach_vm_region(task, &addr, &size, VM_REGION_BASIC_INFO_64,
                       (vm_region_info_t)&info, &info_count, &object_name) == KERN_SUCCESS) {
        /* Check if text is writable (shouldn't be) */
        if (info.protection & VM_PROT_WRITE) {
            return 1;
        }
    }
#endif
    return 0;
}

/* 4c. Return address verification (simple CFI) */
static MIND_NOINLINE int _check_return_address(void) {
#if defined(__x86_64__) && !defined(MIND_PLATFORM_WINDOWS)
    void *ret_addr = __builtin_return_address(0);
    Dl_info info;

    if (dladdr(ret_addr, &info)) {
        /* Return address should be in our binary or libc */
        if (info.dli_fname == NULL) return 1;

        /* Should be in a legitimate library */
        if (strstr(info.dli_fname, "libc") == NULL &&
            strstr(info.dli_fname, "libmind") == NULL &&
            strstr(info.dli_fname, "nikola") == NULL &&
            strstr(info.dli_fname, ".nikolachess") == NULL) {
            /* Check it's not from an injected library */
            char frida[8];
            _decode_str(_s_frida, frida, sizeof(frida));
            if (strstr(info.dli_fname, frida)) {
                return 1;
            }
        }
    }
#endif
    return 0;
}

/*
 * LAYER 5: Background Monitoring Thread
 */

#ifdef MIND_PLATFORM_WINDOWS
static DWORD WINAPI _monitor_thread_func(LPVOID arg) {
    (void)arg;

    while (_monitor_running) {
        /* Periodic integrity checks */
        int threat = 0;

        threat += _check_tracer_pid() ? 100 : 0;
        threat += _check_execution_context() ? 100 : 0;

        if (threat >= 100) {
            ExitProcess(99);
        }

        /* Sleep 500ms between checks */
        Sleep(500);
    }

    return 0;
}

static void _start_monitor_thread(void) {
    if (_monitor_thread != NULL) return;

    _monitor_running = 1;
    _monitor_thread = CreateThread(NULL, 0, _monitor_thread_func, NULL, 0, NULL);
}

#else /* Unix */

static void* _monitor_thread_func(void *arg) {
    (void)arg;

    while (_monitor_running) {
        /* Periodic integrity checks */
        int threat = 0;

        threat += _check_tracer_pid() ? 100 : 0;
        threat += _check_code_integrity() ? 100 : 0;
        threat += _check_memory_permissions() ? 80 : 0;
        threat += _check_execution_context() ? 100 : 0;

        if (threat >= 100) {
            _exit(99);
        }

        /* Sleep 500ms between checks */
        struct timespec ts = {0, 500000000};
        nanosleep(&ts, NULL);
    }

    return NULL;
}

static void _start_monitor_thread(void) {
    if (_monitor_thread != 0) return;

    _monitor_running = 1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&_monitor_thread, &attr, _monitor_thread_func, NULL);
    pthread_attr_destroy(&attr);
}

#endif /* MIND_PLATFORM_WINDOWS */

/*
 * MASTER PROTECTION CHECK
 */

static int _protection_check_all(void) {
    int score = 0;

    /* CRITICAL: Execution context - MUST be NikolaChess */
    if (_check_execution_context()) score += 100;

    /* Layer 1: Anti-debugging (critical) */
    if (_check_tracer_pid()) score += 100;
    if (_check_timing_anomaly()) score += 80;
    if (_check_parent_process()) score += 90;
    if (_check_breakpoint_trap()) score += 70;

    /* Layer 2: Environment (important) */
    if (_check_ld_preload()) score += 60;
    if (_check_ld_audit_debug()) score += 60;
    if (_check_debugger_libs()) score += 50;
    if (_check_debugger_processes()) score += 40;
    if (_check_mem_access()) score += 50;
    if (_check_aslr_disabled()) score += 40;

#ifdef __APPLE__
    if (_check_dyld_insert()) score += 60;
    if (_check_exception_ports()) score += 80;
    if (_check_task_info()) score += 50;
#endif

    /* Layer 3: Anti-tampering (critical) */
    if (_check_code_integrity()) score += 100;
    if (_check_got_hooks()) score += 90;

    /* Layer 4: Memory protection */
    if (_check_memory_permissions()) score += 80;
    if (_check_return_address()) score += 70;

    /* Core dump prevention */
    _check_core_dumps();

    /* VM detection - lower priority, some legitimate use */
    if (_check_virtual_machine()) score += 20;

    return score;
}

/*
 * PUBLIC API
 */

/* Get monotonic time in nanoseconds */
static uint64_t _get_monotonic_time(void) {
#ifdef MIND_PLATFORM_WINDOWS
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

/* Get current process ID */
static uint32_t _get_process_id(void) {
#ifdef MIND_PLATFORM_WINDOWS
    return (uint32_t)GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

#ifdef MIND_PLATFORM_WINDOWS
/* Windows DLL entry point for initialization */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        if (_protection_initialized) return TRUE;

        /* Randomize polymorphic key */
        _poly_key = (uint32_t)(_get_monotonic_time() ^ _get_process_id());

        /* Initial protection check */
        int threat_score = _protection_check_all();

        if (threat_score >= 100) {
            ExitProcess(99);
        } else if (threat_score >= 50) {
            ExitProcess(99);
        }

        /* Start background monitoring thread */
        _start_monitor_thread();

        _protection_initialized = 1;
    }
    return TRUE;
}

#else /* Unix constructor */

MIND_CONSTRUCTOR MIND_HIDDEN
static void _protection_init(void) {
    if (_protection_initialized) return;

    /* Randomize polymorphic key */
    _poly_key = (uint32_t)(_get_monotonic_time() ^ _get_process_id());

    /* Store text section info for integrity monitoring */
#ifdef MIND_PLATFORM_LINUX
    Dl_info info;
    if (dladdr((void*)_protection_init, &info)) {
        _text_base = info.dli_fbase;
        /* Estimate text size from ELF header */
        Elf64_Ehdr *ehdr = (Elf64_Ehdr*)info.dli_fbase;
        if (ehdr->e_ident[EI_MAG0] == ELFMAG0) {
            Elf64_Shdr *shdr = (Elf64_Shdr*)((char*)info.dli_fbase + ehdr->e_shoff);
            char *strtab = (char*)info.dli_fbase + shdr[ehdr->e_shstrndx].sh_offset;
            for (int i = 0; i < ehdr->e_shnum; i++) {
                if (strcmp(strtab + shdr[i].sh_name, ".text") == 0) {
                    _text_size = shdr[i].sh_size;
                    break;
                }
            }
        }
    }
#endif

#ifdef MIND_PLATFORM_MACOS
    /* Get Mach-O header for text section */
    const struct mach_header_64 *header = NULL;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        Dl_info info;
        if (dladdr((void*)_protection_init, &info)) {
            if (strstr(info.dli_fname, "libmind") || strstr(info.dli_fname, "nikola")) {
                header = (const struct mach_header_64*)_dyld_get_image_header(i);
                _text_base = (void*)header;
                break;
            }
        }
    }
#endif

    /* Compute initial code checksum */
    _code_checksum = _compute_code_checksum();

    /* Prevent core dumps */
    _check_core_dumps();

    /* Initial protection check */
    int threat_score = _protection_check_all();

    if (threat_score >= 100) {
        /* Critical threat - exit immediately */
        _exit(99);
    } else if (threat_score >= 50) {
        /* Moderate threat - production should also exit */
        _exit(99);
    }

    /* Start background monitoring thread */
    _start_monitor_thread();

    _protection_initialized = 1;
}

#endif /* MIND_PLATFORM_WINDOWS */

/* Heartbeat - call periodically during execution */
static inline int mind_protection_heartbeat(void) {
    uint64_t now = _get_monotonic_time();

    /* Rate limit checks to every 100ms */
    if (now - _last_heartbeat < 100000000ULL) {
        return 0;
    }
    _last_heartbeat = now;

    int threat_score = _protection_check_all();

    if (threat_score >= 50) {
        return -1;
    }

    return 0;
}

#endif /* MIND_PROTECTION_H */
