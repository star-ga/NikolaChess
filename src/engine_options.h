#pragma once
#include <string>
#include <vector>

namespace nikola {

struct EngineOptions {
    int MultiPV = 1;           // 1..8
    bool LimitStrength = false;
    int Strength = 20;         // 0..20 → caps depth = 1 + Strength
    std::string SyzygyPath;    // WDL+DTZ path (optional)
    bool UCI_ShowWDL = false;
    int HashMB = 64;           // TT soft budget
    int MoveOverhead = 50;     // ms
    int Threads = 1;           // placeholder (root‑stable only in this patch)
};

// Global accessor (simple singleton for UCI layer)
EngineOptions& opts();

// Utility: parse tokens like [name, MultiPV, value, 3, ...]
void set_option_from_tokens(const std::vector<std::string>& tokens);

// Print ID + options block
void print_id_and_options();

// isready hook (e.g., open tablebases lazily)
void on_isready();

} // namespace nikola
