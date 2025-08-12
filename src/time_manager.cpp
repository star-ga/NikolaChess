#include "time_manager.h"
#include <algorithm>

namespace nikola {

int compute_time_budget(const Board&, bool whiteToMove,
                        int wtime, int btime, int winc, int binc,
                        int movestogo, int overhead_ms, double safety) {
    if (wtime < 0 || btime < 0) return -1; // infinite
    int remain = whiteToMove ? wtime : btime;
    int inc    = whiteToMove ? winc  : binc;

    long long per = 0;
    if (movestogo > 0) {
        per = remain / std::max(1, movestogo);
    } else {
        per = (long long)(remain * 0.02) + inc; // 2% of remaining + increment
    }
    per -= overhead_ms;
    per = (long long)(per * (1.0 - safety));
    if (per < 1) per = 1;
    return (int)per;
}

}
