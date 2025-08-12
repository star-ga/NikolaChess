// Minimal bridge providing stub implementations of the Fathom Syzygy tablebase
// API when the actual library is not linked.  These weak functions allow the
// engine to compile without the tablebase files while still permitting unit
// tests to supply their own implementations.

#include <cstdint>

extern "C" int tb_init(const char*) { return 0; }

extern "C" unsigned tb_probe_wdl(
    uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t, unsigned, unsigned, unsigned, bool) {
    return 2; // unknown
}

extern "C" unsigned tb_probe_root(
    uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t, unsigned, unsigned, unsigned, bool,
    unsigned*) {
    return 2; // unknown
}

