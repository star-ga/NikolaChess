#include "polyglot.h"
#include "board.h"
#include <cassert>
#include <iostream>
#include <cstdio>

int main() {
    using namespace nikola;
    Board b = initBoard();
    Move m{};
    m.fromRow = 6; m.fromCol = 4; m.toRow = 4; m.toCol = 4; // e2e4
    m.captured = EMPTY; m.promotedTo = 0;
    addBookEntry(b, m, 10, 0);
    bool ok = saveBook("test.book");
    assert(ok);
    // Enable the book and ensure probing returns the stored move.
    setUseBook(true);
    setBookFile("test.book");
    auto res = probeBook(b);
    assert(res.has_value());
    assert(res->fromRow == m.fromRow && res->fromCol == m.fromCol &&
           res->toRow == m.toRow && res->toCol == m.toCol);
    std::remove("test.book");
    std::cout << "bookgen tests passed\n";
    return 0;
}
