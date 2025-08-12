#include <gtest/gtest.h>
#include "board.h"

TEST(MoveGenerationTest, StartPositionHasTwentyMoves) {
    nikola::Board b = nikola::initBoard();
    auto moves = nikola::generateMoves(b);
    EXPECT_EQ(moves.size(), 20u);
}
