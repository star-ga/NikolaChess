
#include "tt_sharded.h"
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstdlib>
#include <atomic>
#include <memory>

namespace {
// Each shard holds a mutex and an unordered_map.  The struct itself is not
// movable because of the mutex, so we store pointers to shards in the global
// vector to allow resizing.
struct Shard {
    std::mutex m;
    std::unordered_map<uint64_t, TTEntry> map;
};
static std::vector<std::unique_ptr<Shard>> g_shards;
static std::atomic<bool> g_inited{false};

static void ensure_init() {
    if (g_inited.load(std::memory_order_acquire)) return;
    g_shards.resize(64);
    for (auto& p : g_shards) p = std::make_unique<Shard>();
    g_inited.store(true, std::memory_order_release);
}

static std::size_t shard_index(uint64_t key) {
    return ((std::size_t)(key >> 32) ^ (std::size_t)key) % g_shards.size();
}
}

void tt_set_shards(std::size_t n) {
    if (n == 0) n = 1;
    ensure_init();
    // Rebuild shards (non-thread-safe; call at startup or with global stop)
    std::vector<std::unique_ptr<Shard>> fresh(n);
    for (auto& p : fresh) p = std::make_unique<Shard>();
    g_shards.swap(fresh);
}

void tt_configure_from_env() {
    ensure_init();
    if (const char* env = std::getenv("NIKOLA_TT_SHARDS")) {
        std::size_t n = std::strtoull(env, nullptr, 10);
        if (n > 0 && n < 4096) tt_set_shards(n);
    }
}

bool tt_lookup(uint64_t key, TTEntry& out) {
    ensure_init();
    auto idx = shard_index(key);
    auto& s = *g_shards[idx];
    std::lock_guard<std::mutex> lock(s.m);
    auto it = s.map.find(key);
    if (it == s.map.end()) return false;
    out = it->second;
    return true;
}

void tt_store(uint64_t key, const TTEntry& in) {
    ensure_init();
    auto idx = shard_index(key);
    auto& s = *g_shards[idx];
    std::lock_guard<std::mutex> lock(s.m);
    auto it = s.map.find(key);
    if (it == s.map.end()) {
        s.map.emplace(key, in);
    } else {
        // Prefer deeper entries; otherwise replace.
        if (in.depth >= it->second.depth) it->second = in;
    }
}

void tt_clear() {
    ensure_init();
    for (auto& p : g_shards) {
        auto& s = *p;
        std::lock_guard<std::mutex> lock(s.m);
        s.map.clear();
    }
}

std::size_t tt_shard_count() {
    ensure_init();
    return g_shards.size();
}

std::size_t tt_total_entries() {
    ensure_init();
    std::size_t total = 0;
    for (auto& p : g_shards) {
        auto& s = *p;
        std::lock_guard<std::mutex> lock(s.m);
        total += s.map.size();
    }
    return total;
}
