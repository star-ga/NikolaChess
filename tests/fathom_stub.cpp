#include <cstdint>

unsigned last_castling = 0;
unsigned last_ep = 0;

extern "C" int tb_init(const char*) { return 1; }

extern "C" unsigned tb_probe_wdl(
    uint64_t white,
    uint64_t black,
    uint64_t kings,
    uint64_t queens,
    uint64_t rooks,
    uint64_t bishops,
    uint64_t knights,
    uint64_t pawns,
    unsigned rule50,
    unsigned castling,
    unsigned ep,
    bool turn) {
    last_castling = castling;
    last_ep = ep;
    if (castling)
        return 4; // TB_WIN
    if (ep)
        return 0; // TB_LOSS
    return 2;     // TB_DRAW
}

extern "C" unsigned tb_probe_root(
    uint64_t white,
    uint64_t black,
    uint64_t kings,
    uint64_t queens,
    uint64_t rooks,
    uint64_t bishops,
    uint64_t knights,
    uint64_t pawns,
    unsigned rule50,
    unsigned castling,
    unsigned ep,
    bool turn,
    unsigned* results) {
    last_castling = castling;
    last_ep = ep;
    if (castling)
        return 4; // TB_WIN
    if (ep)
        return 0; // TB_LOSS
    return 2;     // TB_DRAW
}
