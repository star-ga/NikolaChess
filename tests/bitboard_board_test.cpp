#include <gtest/gtest.h>
#include "board.h"

TEST(BitboardInitTest, StartingPositionMasks) {
    nikola::Board b = nikola::initBoard();
    EXPECT_EQ(nikola::bb_popcount(b.bitboards.whiteMask), 16);
    EXPECT_EQ(nikola::bb_popcount(b.bitboards.blackMask), 16);
    EXPECT_EQ(nikola::bb_popcount(b.bitboards.occupied), 32);
}
