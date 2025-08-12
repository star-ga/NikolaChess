#include <gtest/gtest.h>
#include "board.h"
#include "search.h"

TEST(SearchTest, BestMoveIsLegal) {
    nikola::Board b = nikola::initBoard();
    nikola::Move m = nikola::findBestMove(b, 1, 100);
    auto moves = nikola::generateMoves(b);
    bool found = false;
    for (const auto& mv : moves) {
        if (mv.fromRow == m.fromRow && mv.fromCol == m.fromCol &&
            mv.toRow == m.toRow && mv.toCol == m.toCol && mv.promotedTo == m.promotedTo) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
