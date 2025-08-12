void uci_go(const Board& current, const std::vector<std::string>& tokens) {
    EngineOptions& o = opts();

    int searchDepth = to_i(tokens, "depth", -1);
    int movetime    = to_i(tokens, "movetime", -1);
    int wtime       = to_i(tokens, "wtime", -1);
    int btime       = to_i(tokens, "btime", -1);
    int winc        = to_i(tokens, "winc", 0);
    int binc        = to_i(tokens, "binc", 0);
    int mtg         = to_i(tokens, "movestogo", 0);

    // Strength cap (preserves existing master semantics)
    if (getLimitStrength()) {
        int cap = 1 + getStrength();
        searchDepth = (searchDepth < 0) ? cap : std::min(searchDepth, cap);
    }

    // Time limit: movetime takes precedence; otherwise use clock-based budget.
    int timeBudgetMs = -1;
    if (movetime >= 0) {
        // Fixed movetime: no safety buffer
        timeBudgetMs = movetime;
    } else {
        // Use 10% safety on clock-based limits; clamp to at least 50ms
        timeBudgetMs = compute_time_budget(
            current, current.whiteToMove, wtime, btime, winc, binc, mtg,
            o.MoveOverhead, 0.10
        );
        if (timeBudgetMs > 0 && timeBudgetMs < 50) timeBudgetMs = 50;
    }

    // Run root MultiPV with the computed budget (or infinite if -1)
    auto results = search_multipv(current, std::max(1, o.MultiPV), searchDepth, timeBudgetMs);

    // Emit UCI "info" lines
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& R = results[i];
        std::ostringstream line;
        line << "info multipv " << (i + 1) << ' ';
        if (std::abs(R.scoreCentipawns) > 29000) {
            int mate_in = (30000 - std::abs(R.scoreCentipawns) + 1) / 2;
            if (R.scoreCentipawns < 0) mate_in = -mate_in;
            line << "score mate " << mate_in << ' ';
        } else {
            line << "score cp " << R.scoreCentipawns << ' ';
        }
        if (o.UCI_ShowWDL) {
            int wdl = probeWDL(current);            // 1 win, 0 draw, -1 loss, 2 unknown
            if (wdl != 2) {
                int win = 0, draw = 0, loss = 0;
                if (wdl == 1) win = 1000;
                else if (wdl == 0) draw = 1000;
                else loss = 1000;
                line << "wdl " << win << ',' << draw << ',' << loss << ' ';
            }
        }
        line << "pv";
        int count = 0;
        for (const auto& m : R.pv) {
            line << ' ' << move_to_uci(current, m);
            if (++count >= 60) break;
        }
        std::cout << line.str() << std::endl;
    }

    // bestmove
    if (!results.empty()) {
        std::cout << "bestmove " << move_to_uci(current, results[0].firstMove) << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl;
    }
}
