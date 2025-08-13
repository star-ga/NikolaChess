#include "polyglot.h"
#include "board.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main() {
    using namespace nikola;
    Board b = initBoard();
    Move m{};
    m.fromRow = 6; m.fromCol = 4; m.toRow = 4; m.toCol = 4; // e2e4
    m.captured = EMPTY; m.promotedTo = 0;
    addBookEntry(b, m, 10, 0);
    bool ok = saveBook("test.book");
    assert(ok);
    std::ifstream f("test.book", std::ios::binary);
    assert(f);
    f.seekg(0, std::ios::end);
    assert(f.tellg() > 0);
    std::cout << "bookgen tests passed\n";
    return 0;
}
