#include <gtest/gtest.h>
#include "nnue.h"
#include "board.h"

TEST(NNUELargeNetTest, EvaluateStartPos) {
    nikola::Board b = nikola::initBoard();
    // Construct a larger network than default
    nikola::NNUE net(12*64, 512, 64);
    int score = net.evaluate(b.bitboards);
    // Score should be finite
    EXPECT_GT(score, -100000);
    EXPECT_LT(score, 100000);
}
