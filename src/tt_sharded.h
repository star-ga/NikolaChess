
#pragma once
#include <cstdint>
#include <optional>
#include "tt_entry.h"

// Simple sharded transposition table with striped locks.
// Number of shards defaults to 64, configurable via env NIKOLA_TT_SHARDS.

void tt_configure_from_env();
void tt_set_shards(std::size_t n);
bool tt_lookup(uint64_t key, TTEntry& out);
void tt_store(uint64_t key, const TTEntry& in);
void tt_clear();
std::size_t tt_shard_count();
std::size_t tt_total_entries();
