#include "engine_options.h"
#include <iostream>
#include <algorithm>
#include <cctype>

namespace nikola {

static EngineOptions G;

EngineOptions& opts() { return G; }

static inline std::string lower(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

void set_option_from_tokens(const std::vector<std::string>& t) {
    // Expect sequence: setoption name <NAME> [value <VAL>]
    auto itn = std::find(t.begin(), t.end(), "name");
    if (itn == t.end()) return;
    std::string name;
    std::string value;
    auto itv = std::find(t.begin(), t.end(), "value");
    if (itv != t.end()) {
        name = lower(std::string(itn+1, itv));
        value = std::string(itv+1, t.end());
    } else {
        name = lower(std::string(itn+1, t.end()));
        value = "true"; // toggles
    }

    if (name == "multipv") {
        int v = std::clamp(std::stoi(value), 1, 8);
        G.MultiPV = v;
    } else if (name == "limitstrength") {
        std::string v = lower(value);
        G.LimitStrength = (v == "true" || v == "1" || v == "on" || v == "yes");
    } else if (name == "strength") {
        int v = std::clamp(std::stoi(value), 0, 20);
        G.Strength = v;
    } else if (name == "syzygypath") {
        G.SyzygyPath = value;
    } else if (name == "uci_showwdl") {
        std::string v = lower(value);
        G.UCI_ShowWDL = (v == "true" || v == "1" || v == "on" || v == "yes");
    } else if (name == "hash") {
        int v = std::clamp(std::stoi(value), 4, 4096);
        G.HashMB = v;
    } else if (name == "moveoverhead") {
        int v = std::clamp(std::stoi(value), 0, 1000);
        G.MoveOverhead = v;
    } else if (name == "threads") {
        int v = std::clamp(std::stoi(value), 1, 128);
        G.Threads = v;
    }
}

void print_id_and_options() {
    std::cout << "id name SupercomputerChessEngine v20" << std::endl;
    std::cout << "id author CPUTER Inc." << std::endl;
    std::cout << "option name MultiPV type spin default 1 min 1 max 8" << std::endl;
    std::cout << "option name LimitStrength type check default false" << std::endl;
    std::cout << "option name Strength type spin default 20 min 0 max 20" << std::endl;
    std::cout << "option name SyzygyPath type string default" << std::endl;
    std::cout << "option name UCI_ShowWDL type check default false" << std::endl;
    std::cout << "option name Hash type spin default 64 min 4 max 4096" << std::endl;
    std::cout << "option name MoveOverhead type spin default 50 min 0 max 1000" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
    std::cout << "uciok" << std::endl;
}

void on_isready() {
    // If you want to lazy-open Syzygy here, do it (calls in multipv_search will also probe).
    std::cout << "readyok" << std::endl;
}

} // namespace nikola
